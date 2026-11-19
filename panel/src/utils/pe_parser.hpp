#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>

namespace l4g::pe {

struct SectionInfo {
    std::string name;
    uint32_t virtual_address = 0;
    uint32_t virtual_size = 0;
    uint32_t raw_address = 0;
    uint32_t raw_size = 0;
    uint32_t characteristics = 0;
};

struct ImportInfo {
    std::string dll_name;
    std::string function_name;
    uint16_t ordinal = 0;
    bool is_ordinal = false;
    uint64_t iat_address = 0;
};

struct RelocationInfo {
    uint64_t address = 0;
    uint16_t type = 0;
};

class PEParser {
public:
    explicit PEParser(const uint8_t* data, size_t size);
    explicit PEParser(const std::vector<uint8_t>& data);

    bool is_valid() const { return valid_; }

    // Headers
    const IMAGE_DOS_HEADER* dos_header() const { return dos_; }
    const IMAGE_NT_HEADERS* nt_headers() const { return nt_; }
    uint64_t get_image_base() const { return nt_ ? nt_->OptionalHeader.ImageBase : 0; }
    uint32_t get_entry_point() const { return nt_ ? nt_->OptionalHeader.AddressOfEntryPoint : 0; }
    uint32_t get_size_of_image() const { return nt_ ? nt_->OptionalHeader.SizeOfImage : 0; }

    // Sections
    std::vector<SectionInfo> get_sections() const;

    // Imports
    std::vector<ImportInfo> get_imports() const;

    // Relocations
    std::vector<RelocationInfo> get_relocations(uintptr_t delta) const;

    // Map to target process memory
    void* map_to_memory(HANDLE process);

    // Unmap from target process
    bool unmap_from_memory(void* base, HANDLE process);

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0;
    const IMAGE_DOS_HEADER* dos_ = nullptr;
    const IMAGE_NT_HEADERS* nt_ = nullptr;
    bool valid_ = false;

    bool parse_headers();
    bool parse_sections(std::vector<SectionInfo>& out) const;
    bool parse_imports(std::vector<ImportInfo>& out) const;
    bool parse_relocations(uintptr_t delta, std::vector<RelocationInfo>& out) const;
};

} // namespace l4g::pe