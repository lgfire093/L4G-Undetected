#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>

namespace l4g::pe {

struct ExportEntry {
    std::string name;
    uintptr_t address;
    uint16_t ordinal;
};

struct ImportEntry {
    std::string dll_name;
    std::string function_name;
    uintptr_t iat_address;
    uint16_t ordinal;
    bool is_ordinal;
};

struct RelocationEntry {
    uintptr_t address;
    uint16_t type;
};

struct SectionInfo {
    std::string name;
    uintptr_t virtual_address;
    size_t virtual_size;
    size_t raw_size;
    uintptr_t raw_address;
    DWORD characteristics;
};

class PEParser {
public:
    explicit PEParser(const uint8_t* base, size_t size);
    explicit PEParser(const std::vector<uint8_t>& data);
    
    bool is_valid() const { return valid_; }
    bool is_64bit() const { return is_64bit_; }
    uintptr_t get_entry_point() const { return entry_point_; }
    uintptr_t get_image_base() const { return image_base_; }
    size_t get_image_size() const { return image_size_; }
    
    std::vector<ExportEntry> get_exports() const;
    std::vector<ImportEntry> get_imports() const;
    std::vector<RelocationEntry> get_relocations(uintptr_t delta) const;
    std::vector<SectionInfo> get_sections() const;
    
    std::optional<uintptr_t> get_export_address(const std::string& name) const;
    std::optional<uintptr_t> get_export_address(uint16_t ordinal) const;
    
    void* map_to_memory(HANDLE process = GetCurrentProcess()) const;
    static void unmap_from_memory(void* base, HANDLE process = GetCurrentProcess());

private:
    bool valid_ = false;
    bool is_64bit_ = false;
    uintptr_t entry_point_ = 0;
    uintptr_t image_base_ = 0;
    size_t image_size_ = 0;
    const uint8_t* base_ = nullptr;
    size_t size_ = 0;
    
    const IMAGE_NT_HEADERS* nt_headers_ = nullptr;
    const IMAGE_OPTIONAL_HEADER* opt_header_ = nullptr;
};

} // namespace l4g::pe