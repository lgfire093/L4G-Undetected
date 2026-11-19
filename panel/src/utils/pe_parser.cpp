#include "pe_parser.hpp"
#include <cstring>

namespace l4g::pe {

PEParser::PEParser(const uint8_t* data, size_t size) : data_(data), size_(size) {
    parse_headers();
}

PEParser::PEParser(const std::vector<uint8_t>& data) : data_(data.data()), size_(data.size()) {
    parse_headers();
}

bool PEParser::parse_headers() {
    if (size_ < sizeof(IMAGE_DOS_HEADER)) return false;
    dos_ = reinterpret_cast<const IMAGE_DOS_HEADER*>(data_);
    if (dos_->e_magic != IMAGE_DOS_SIGNATURE) return false;
    if (size_ < dos_->e_lfanew + sizeof(IMAGE_NT_HEADERS)) return false;
    nt_ = reinterpret_cast<const IMAGE_NT_HEADERS*>(data_ + dos_->e_lfanew);
    if (nt_->Signature != IMAGE_NT_SIGNATURE) return false;
    valid_ = true;
    return true;
}

std::vector<SectionInfo> PEParser::get_sections() const {
    std::vector<SectionInfo> sections;
    parse_sections(sections);
    return sections;
}

bool PEParser::parse_sections(std::vector<SectionInfo>& out) const {
    if (!valid_) return false;
    const IMAGE_SECTION_HEADER* sect = IMAGE_FIRST_SECTION(nt_);
    for (int i = 0; i < nt_->FileHeader.NumberOfSections; ++i) {
        SectionInfo info;
        info.name = std::string(reinterpret_cast<const char*>(sect[i].Name), 8);
        info.name = info.name.substr(0, info.name.find('\0'));
        info.virtual_address = sect[i].VirtualAddress;
        info.virtual_size = sect[i].Misc.VirtualSize;
        info.raw_address = sect[i].PointerToRawData;
        info.raw_size = sect[i].SizeOfRawData;
        info.characteristics = sect[i].Characteristics;
        out.push_back(info);
    }
    return true;
}

std::vector<ImportInfo> PEParser::get_imports() const {
    std::vector<ImportInfo> imports;
    parse_imports(imports);
    return imports;
}

bool PEParser::parse_imports(std::vector<ImportInfo>& out) const {
    if (!valid_) return false;
    auto import_dir = nt_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (import_dir.VirtualAddress == 0 || import_dir.Size == 0) return true;

    const IMAGE_IMPORT_DESCRIPTOR* imp = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(data_ + import_dir.VirtualAddress);
    for (size_t i = 0; imp[i].Name; ++i) {
        const char* dll_name = reinterpret_cast<const char*>(data_ + imp[i].Name);
        const IMAGE_THUNK_DATA* thunk = reinterpret_cast<const IMAGE_THUNK_DATA*>(data_ + imp[i].OriginalFirstThunk);
        const IMAGE_THUNK_DATA* iat = reinterpret_cast<const IMAGE_THUNK_DATA*>(data_ + imp[i].FirstThunk);

        for (size_t k = 0; thunk[k].u1.AddressOfData; ++k) {
            ImportInfo info;
            info.dll_name = dll_name;
            info.iat_address = imp[i].FirstThunk + k * sizeof(uintptr_t);
            if (thunk[k].u1.Ordinal & IMAGE_ORDINAL_FLAG) {
                info.is_ordinal = true;
                info.ordinal = static_cast<uint16_t>(thunk[k].u1.Ordinal & 0xFFFF);
            } else {
                auto by_name = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(data_ + thunk[k].u1.AddressOfData);
                info.function_name = reinterpret_cast<const char*>(by_name->Name);
            }
            out.push_back(info);
        }
    }
    return true;
}

std::vector<RelocationInfo> PEParser::get_relocations(uintptr_t delta) const {
    std::vector<RelocationInfo> relocs;
    parse_relocations(delta, relocs);
    return relocs;
}

bool PEParser::parse_relocations(uintptr_t delta, std::vector<RelocationInfo>& out) const {
    if (!valid_ || delta == 0) return true;
    auto reloc_dir = nt_->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (reloc_dir.VirtualAddress == 0 || reloc_dir.Size == 0) return true;

    const IMAGE_BASE_RELOCATION* reloc = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(data_ + reloc_dir.VirtualAddress);
    const uint8_t* end = data_ + reloc_dir.VirtualAddress + reloc_dir.Size;

    while (reinterpret_cast<const uint8_t*>(reloc) < end) {
        uint32_t page = reloc->VirtualAddress;
        uint32_t count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        const WORD* entries = reinterpret_cast<const WORD*>(reloc + 1);
        for (uint32_t i = 0; i < count; ++i) {
            WORD entry = entries[i];
            WORD type = entry >> 12;
            WORD offset = entry & 0xFFF;
            if (type == IMAGE_REL_BASED_DIR64 || type == IMAGE_REL_BASED_HIGHLOW) {
                RelocationInfo info;
                info.address = page + offset;
                info.type = type;
                out.push_back(info);
            }
        }
        reloc = reinterpret_cast<const IMAGE_BASE_RELOCATION*>(reinterpret_cast<const uint8_t*>(reloc) + reloc->SizeOfBlock);
    }
    return true;
}

void* PEParser::map_to_memory(HANDLE process) {
    if (!valid_) return nullptr;
    void* base = VirtualAllocEx(process, reinterpret_cast<void*>(nt_->OptionalHeader.ImageBase),
                                nt_->OptionalHeader.SizeOfImage,
                                MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!base) {
        base = VirtualAllocEx(process, nullptr, nt_->OptionalHeader.SizeOfImage,
                              MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    }
    if (!base) return nullptr;

    // Write headers
    SIZE_T written = 0;
    WriteProcessMemory(process, base, data_, nt_->OptionalHeader.SizeOfHeaders, &written);

    // Write sections
    const IMAGE_SECTION_HEADER* sect = IMAGE_FIRST_SECTION(nt_);
    for (int i = 0; i < nt_->FileHeader.NumberOfSections; ++i) {
        if (sect[i].SizeOfRawData > 0) {
            WriteProcessMemory(process,
                               reinterpret_cast<uint8_t*>(base) + sect[i].VirtualAddress,
                               data_ + sect[i].PointerToRawData,
                               sect[i].SizeOfRawData, &written);
        }
    }
    return base;
}

bool PEParser::unmap_from_memory(void* base, HANDLE process) {
    return VirtualFreeEx(process, base, 0, MEM_RELEASE);
}

} // namespace l4g::pe