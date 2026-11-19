#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>

namespace l4g::kernel {

struct DriverMapResult {
    bool success = false;
    uint64_t kernel_base = 0;
    HANDLE device_handle = nullptr;
    std::string error;
};

class DriverMapper {
public:
    // Manually map unsigned driver into kernel via BYOVD
    static DriverMapResult map_driver(const std::vector<uint8_t>& driver_data);
    
    // Unmap driver
    static bool unmap_driver(uint64_t kernel_base);
    
    // Driver entry point call
    static bool call_driver_entry(uint64_t kernel_base, uintptr_t entry_point, void* context);
    
    // Kernel memory allocation
    static bool allocate_kernel_memory(size_t size, uint64_t& out_address);
    static bool free_kernel_memory(uint64_t address);
    
    // Kernel read/write via mapped driver
    static bool kernel_read(uint64_t address, void* buffer, size_t size);
    static bool kernel_write(uint64_t address, const void* buffer, size_t size);
    
private:
    static bool map_sections(uint64_t kernel_base, const std::vector<uint8_t>& driver_data);
    static bool apply_kernel_relocations(uint64_t kernel_base, const std::vector<uint8_t>& driver_data);
    static bool resolve_kernel_imports(uint64_t kernel_base, const std::vector<uint8_t>& driver_data);
    static bool protect_kernel_sections(uint64_t kernel_base, const std::vector<uint8_t>& driver_data);
};

} // namespace l4g::kernel