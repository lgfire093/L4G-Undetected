#include "ac_neutralizer.hpp"
#include "byovd.hpp"
#include "dkom.hpp"
#include "../utils/syscalls.hpp"
#include "../utils/nt_defs.hpp"
#include <windows.h>
#include <vector>
#include <string>
#include <tlhelp32.h>
#include <stdio.h>
#include <algorithm>

#pragma comment(lib, "ntdll.lib")

namespace l4g::kernel {

// winternl.h stops short of this member in the user-mode SDK enum.
static constexpr SYSTEM_INFORMATION_CLASS kSystemModuleInformation =
    static_cast<SYSTEM_INFORMATION_CLASS>(11);

static std::string narrow(const wchar_t* wide) {
    if (!wide) return {};
    int need = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (need <= 1) return {};
    std::string out((size_t)need - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), need, nullptr, nullptr);
    return out;
}

const ACNeutralizer::ACDriver ACNeutralizer::KNOWN_AC_DRIVERS[] = {
    {L"BEDaisy.sys", L"\\Device\\BEDaisy", L"BEDaisy", 0, 0, 0},
    {L"BEKernel.sys", L"\\Device\\BEKernel", L"BEKernel", 0, 0, 0},
    {L"EasyAntiCheat.sys", L"\\Device\\EasyAntiCheat", L"EasyAntiCheat", 0, 0, 0},
    {L"VAC.sys", L"\\Device\\VAC", L"VAC", 0, 0, 0},
    {L"detectac.sys", L"\\Device\\detectac", L"detectac", 0, 0, 0},
    {L"PcCheck.sys", L"\\Device\\PcCheck", L"PcCheck", 0, 0, 0},
    {L"GameGuard.sys", L"\\Device\\GameGuard", L"GameGuard", 0, 0, 0},
    {L"Xigncode3.sys", L"\\Device\\Xigncode3", L"Xigncode3", 0, 0, 0},
};
size_t ACNeutralizer::AC_DRIVER_COUNT = sizeof(KNOWN_AC_DRIVERS) / sizeof(ACDriver);

ACNeutralizerResult ACNeutralizer::neutralize_all() {
    ACNeutralizerResult result;
    
    for (size_t i = 0; i < AC_DRIVER_COUNT; ++i) {
        const auto& ac = KNOWN_AC_DRIVERS[i];

        uint64_t driver_base = 0;
        size_t driver_size = 0;
        if (find_driver_base(ac.name, driver_base, driver_size)) {
            bool ok = false;
            if (wcscmp(ac.name, L"BEDaisy.sys") == 0 || wcscmp(ac.name, L"BEKernel.sys") == 0) {
                ok = neutralize_battleye();
            } else if (wcscmp(ac.name, L"EasyAntiCheat.sys") == 0) {
                ok = neutralize_eac();
            } else if (wcscmp(ac.name, L"VAC.sys") == 0) {
                ok = neutralize_vac();
            } else if (wcscmp(ac.name, L"detectac.sys") == 0) {
                ok = neutralize_detect_ac();
            } else {
                ok = neutralize_generic_pccheck();
            }
            
            if (ok) {
                result.neutralized.push_back(narrow(ac.name));
            }
        }
    }
    
    result.success = !result.neutralized.empty();
    return result;
}

bool ACNeutralizer::find_driver_base(const wchar_t* name, uint64_t& out_base, size_t& out_size) {
    // Use NtQuerySystemInformation with SystemModuleInformation
    ULONG len = 0;
    auto& sys = l4g::syscalls::get_syscall_table();
    if (!sys.NtQuerySystemInformation) return false;

    sys.NtQuerySystemInformation(kSystemModuleInformation, nullptr, 0, &len);
    if (len == 0) return false;

    std::vector<uint8_t> buffer(len + 0x1000);
    if (!NT_SUCCESS(sys.NtQuerySystemInformation(kSystemModuleInformation, buffer.data(), (ULONG)buffer.size(), &len))) {
        return false;
    }

    char narrow[256] = {};
    int wide_len = (int)wcslen(name);
    if (wide_len >= (int)sizeof(narrow)) wide_len = (int)sizeof(narrow) - 1;
    WideCharToMultiByte(CP_ACP, 0, name, wide_len, narrow, (int)sizeof(narrow) - 1, nullptr, nullptr);

    auto* modules = (PL4G_RTL_PROCESS_MODULES)buffer.data();
    for (ULONG i = 0; i < modules->NumberOfModules; ++i) {
        auto& mod = modules->Modules[i];
        const char* mod_name = (const char*)mod.FullPathName + mod.OffsetToFileName;
        if (_stricmp(mod_name, narrow) == 0) {
            out_base = (uint64_t)mod.ImageBase;
            out_size = mod.ImageSize;
            return true;
        }
    }
    return false;
}

bool ACNeutralizer::neutralize_battleye() {
    // BattlEye: remove PsSetCreateProcessNotifyRoutine, PsSetLoadImageNotifyRoutine
    // Patch dispatch routine to return STATUS_SUCCESS
    // Hide our process from their process enumeration
    return remove_notify_routines(L"BEDaisy.sys") && 
           remove_notify_routines(L"BEKernel.sys") &&
           patch_dispatch_routine(L"BEDaisy.sys") &&
           patch_dispatch_routine(L"BEKernel.sys");
}

bool ACNeutralizer::neutralize_eac() {
    return remove_notify_routines(L"EasyAntiCheat.sys") &&
           patch_dispatch_routine(L"EasyAntiCheat.sys") &&
           remove_device_object(L"\\Device\\EasyAntiCheat");
}

bool ACNeutralizer::neutralize_vac() {
    return remove_notify_routines(L"VAC.sys") &&
           patch_dispatch_routine(L"VAC.sys");
}

bool ACNeutralizer::neutralize_detect_ac() {
    // detect.ac: heavy PowerShell monitoring, kernel hooks
    return remove_notify_routines(L"detectac.sys") &&
           patch_dispatch_routine(L"detectac.sys") &&
           remove_device_object(L"\\Device\\detectac") &&
           hide_process_from_ac((HANDLE)(ULONG_PTR)GetCurrentProcessId(), L"detectac.sys");
}

bool ACNeutralizer::neutralize_generic_pccheck() {
    return remove_notify_routines(L"PcCheck.sys") &&
           patch_dispatch_routine(L"PcCheck.sys");
}

bool ACNeutralizer::remove_notify_routines(const wchar_t* driver_name) {
    uint64_t base = 0; size_t size = 0;
    if (!find_driver_base(driver_name, base, size)) return false;
    
    std::vector<uint64_t> routines;
    if (!scan_for_notify_routines(base, size, routines)) return false;
    
    for (uint64_t routine : routines) {
        if (!unregister_notify_routine(routine)) return false;
    }
    return true;
}

bool ACNeutralizer::scan_for_notify_routines(uint64_t driver_base, size_t driver_size, std::vector<uint64_t>& out_routines) {
    // Scan driver .data section for pointers to PsSetCreateProcessNotifyRoutine callbacks
    // This is complex - placeholder
    return false;
}

bool ACNeutralizer::unregister_notify_routine(uint64_t routine_addr) {
    // Call PsSetCreateProcessNotifyRoutineEx with Remove = TRUE
    // Requires kernel call primitive
    return false;
}

bool ACNeutralizer::restore_inline_hooks(const wchar_t* driver_name) {
    // Scan driver code section for hooks (JMP/CALL to our code)
    // Restore original bytes
    return false;
}

bool ACNeutralizer::hide_process_from_ac(HANDLE pid, const wchar_t* driver_name) {
    // DKOM hide from PsActiveProcessHead
    // Also hide from driver's private process list
    return DKOM::hide_process(pid).success;
}

bool ACNeutralizer::remove_device_object(const wchar_t* device_name) {
    // Find and unlink DEVICE_OBJECT from driver's DeviceObject list
    // Requires kernel read/write
    return false;
}

bool ACNeutralizer::patch_dispatch_routine(const wchar_t* driver_name) {
    // Find DRIVER_OBJECT->MajorFunction[IRP_MJ_DEVICE_CONTROL]
    // Patch to return STATUS_SUCCESS
    return false;
}

bool ACNeutralizer::safe_mode_neutralize(const wchar_t* driver_name) {
    // If kernel exploitation fails, use user-mode evasion:
    // - Hook user-mode APIs the AC uses
    // - Spoof return values
    // - Hide from user-mode enumeration
    return false;
}

} // namespace l4g::kernel