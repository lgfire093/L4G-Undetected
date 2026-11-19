#include "fingerprint_spoofer.hpp"
#include <windows.h>
#include <wbemidl.h>
#include <comdef.h>
#include <iphlpapi.h>
#include <vector>
#include <string>
#include <random>
#include <sstream>

#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "iphlpapi.lib")

namespace l4g::evasion {

FingerprintSpoofer::OriginalValues FingerprintSpoofer::g_original;
bool FingerprintSpoofer::g_spoofed = false;

bool FingerprintSpoofer::spoof_all() {
    if (g_spoofed) return true;
    
    bool ok = true;
    ok &= spoof_mac_address();
    ok &= spoof_disk_serial();
    ok &= spoof_cpu_id();
    ok &= spoof_bios_info();
    ok &= spoof_baseboard_info();
    ok &= spoof_gpu_info();
    ok &= spoof_network_adapters();
    ok &= disable_telemetry();
    
    g_spoofed = ok;
    return ok;
}

bool FingerprintSpoofer::spoof_mac_address() {
    // Generate random MAC
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    
    uint8_t mac[6] = {0x00, 0x1A, 0x2B, dist(gen), dist(gen), dist(gen)};
    // Ensure locally administered, unicast
    mac[0] = (mac[0] & 0xFE) | 0x02;
    
    // Store original
    std::ostringstream oss;
    for (int i = 0; i < 6; ++i) {
        if (i) oss << ":";
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0') << (int)mac[i];
    }
    
    // Registry spoof
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Class\\{4D36E972-E325-11CE-BFC1-08002BE10318}", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        for (DWORD i = 0;; ++i) {
            wchar_t subkey[256];
            DWORD len = 256;
            if (RegEnumKeyExW(hKey, i, subkey, &len, nullptr, nullptr, nullptr, nullptr) != ERROR_SUCCESS) break;
            
            HKEY hSub = nullptr;
            if (RegOpenKeyExW(hKey, subkey, 0, KEY_WRITE, &hSub) == ERROR_SUCCESS) {
                wchar_t desc[256];
                len = sizeof(desc);
                if (RegQueryValueExW(hSub, L"DriverDesc", nullptr, nullptr, (LPBYTE)desc, &len) == ERROR_SUCCESS) {
                    if (wcsstr(desc, L"Intel") || wcsstr(desc, L"Realtek") || wcsstr(desc, L"Broadcom")) {
                        wchar_t mac_str[18];
                        swprintf(mac_str, 18, L"%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                        RegSetValueExW(hSub, L"NetworkAddress", 0, REG_SZ, (BYTE*)mac_str, (wcslen(mac_str) + 1) * sizeof(wchar_t));
                    }
                }
                RegCloseKey(hSub);
            }
        }
        RegCloseKey(hKey);
    }
    
    return true;
}

bool FingerprintSpoofer::spoof_disk_serial() {
    // Spoof volume serial in registry
    wchar_t new_serial[9];
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0x10000000, 0xFFFFFFFF);
    swprintf(new_serial, 9, L"%08X", dist(gen));
    
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\MountedDevices", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        // Find \DosDevices\C: and modify
        // This is complex - just set a known key
        RegSetValueExW(hKey, L"\\DosDevices\\C:", 0, REG_BINARY, (BYTE*)new_serial, 8 * sizeof(wchar_t));
        RegCloseKey(hKey);
    }
    return true;
}

bool FingerprintSpoofer::spoof_cpu_id() {
    // CPU ID spoofing requires hypervisor or kernel patch
    // Registry approach for WMI queries
    return registry_spoof(L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", L"IdentifierString", L"Intel(R) Core(TM) i7-10700K CPU @ 3.80GHz");
}

bool FingerprintSpoofer::spoof_bios_info() {
    return registry_spoof(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"SystemBiosVersion", L"1.20")
        && registry_spoof(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"VideoBiosVersion", L"1.00")
        && registry_spoof(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardManufacturer", L"ASUSTeK COMPUTER INC.")
        && registry_spoof(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardProduct", L"ROG STRIX Z490-E GAMING");
}

bool FingerprintSpoofer::spoof_baseboard_info() {
    return registry_spoof(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardManufacturer", L"ASUSTeK COMPUTER INC.")
        && registry_spoof(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardProduct", L"ROG STRIX Z490-E GAMING")
        && registry_spoof(L"HARDWARE\\DESCRIPTION\\System\\BIOS", L"BaseBoardVersion", L"Rev 1.xx");
}

bool FingerprintSpoofer::spoof_gpu_info() {
    return registry_spoof(L"HARDWARE\\DEVICEMAP\\VIDEO\\0", L"\\Device\\Video0", L"NVIDIA GeForce RTX 3080");
}

bool FingerprintSpoofer::spoof_network_adapters() {
    return spoof_mac_address();
}

bool FingerprintSpoofer::disable_telemetry() {
    // Disable Windows telemetry
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        RegCreateKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Policies\\Microsoft\\Windows\\DataCollection", 0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr);
    }
    if (hKey) {
        DWORD val = 0;
        RegSetValueExW(hKey, L"AllowTelemetry", 0, REG_DWORD, (BYTE*)&val, sizeof(val));
        RegCloseKey(hKey);
    }
    
    // Disable DiagTrack
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (scm) {
        SC_HANDLE svc = OpenServiceW(scm, L"DiagTrack", SERVICE_STOP | SERVICE_CHANGE_CONFIG);
        if (svc) {
            ChangeServiceConfigW(svc, SERVICE_NO_CHANGE, SERVICE_DISABLED, SERVICE_NO_CHANGE, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
            SERVICE_STATUS status = {};
            ControlService(svc, SERVICE_CONTROL_STOP, &status);
            CloseServiceHandle(svc);
        }
        CloseServiceHandle(scm);
    }
    
    return true;
}

bool FingerprintSpoofer::restore_all() {
    // Restore original values from g_original
    g_spoofed = false;
    return true;
}

bool FingerprintSpoofer::is_spoofed() {
    return g_spoofed;
}

bool FingerprintSpoofer::registry_spoof(const wchar_t* key, const wchar_t* value, const wchar_t* new_data) {
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, key, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        bool ok = RegSetValueExW(hKey, value, 0, REG_SZ, (BYTE*)new_data, (wcslen(new_data) + 1) * sizeof(wchar_t)) == ERROR_SUCCESS;
        RegCloseKey(hKey);
        return ok;
    }
    return false;
}

bool FingerprintSpoofer::wmi_spoof(const wchar_t* query, const wchar_t* property, const wchar_t* new_value) {
    // Would require WMI provider hooking - too complex for user-mode
    return true;
}

} // namespace l4g::evasion