#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace l4g::evasion {

class FingerprintSpoofer {
public:
    // Spoof all fingerprinting vectors
    static bool spoof_all();
    
    // Individual spoofers
    static bool spoof_mac_address();
    static bool spoof_disk_serial();
    static bool spoof_cpu_id();
    static bool spoof_bios_info();
    static bool spoof_baseboard_info();
    static bool spoof_gpu_info();
    static bool spoof_network_adapters();
    static bool spoof_wmi_queries();
    static bool disable_telemetry();
    
    // Restore original values
    static bool restore_all();
    
    // Check if spoofed
    static bool is_spoofed();
    
private:
    struct OriginalValues {
        std::string mac_address;
        std::string disk_serial;
        std::vector<int> cpu_id[4];
        std::string bios_vendor;
        std::string bios_version;
        std::string baseboard_manufacturer;
        std::string baseboard_product;
        std::string gpu_name;
    };
    
    static OriginalValues g_original;
    static bool g_spoofed;
    
    static bool registry_spoof(const wchar_t* key, const wchar_t* value, const wchar_t* new_data);
    static bool wmi_spoof(const wchar_t* query, const wchar_t* property, const wchar_t* new_value);
};

} // namespace l4g::evasion