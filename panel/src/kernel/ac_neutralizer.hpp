#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>

namespace l4g::kernel {

struct ACNeutralizerResult {
    bool success = false;
    std::vector<std::string> neutralized;
    std::string error;
};

class ACNeutralizer {
public:
    // Neutralize known anti-cheat drivers
    static ACNeutralizerResult neutralize_all();
    
    // Specific neutralizers
    static bool neutralize_battleye();
    static bool neutralize_eac();
    static bool neutralize_vac();
    static bool neutralize_detect_ac();
    static bool neutralize_generic_pccheck();
    
    // Generic neutralization techniques
    static bool remove_notify_routines(const wchar_t* driver_name);
    static bool restore_inline_hooks(const wchar_t* driver_name);
    static bool hide_process_from_ac(HANDLE pid, const wchar_t* driver_name);
    static bool remove_device_object(const wchar_t* device_name);
    static bool patch_dispatch_routine(const wchar_t* driver_name);
    
    // BSOD-safe fallback
    static bool safe_mode_neutralize(const wchar_t* driver_name);
    
    // Anti-cheat driver signatures
    struct ACDriver {
        const wchar_t* name;
        const wchar_t* device_name;
        const wchar_t* service_name;
        uint64_t notify_routine_offset;
        uint64_t dispatch_routine_offset;
        uint64_t device_object_offset;
    };
    
    static const ACDriver KNOWN_AC_DRIVERS[];
    static size_t AC_DRIVER_COUNT;
    
private:
    static bool find_driver_base(const wchar_t* name, uint64_t& out_base, size_t& out_size);
    static bool scan_for_notify_routines(uint64_t driver_base, size_t driver_size, std::vector<uint64_t>& out_routines);
    static bool unregister_notify_routine(uint64_t routine_addr);
};

} // namespace l4g::kernel