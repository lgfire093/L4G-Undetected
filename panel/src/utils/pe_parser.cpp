#include "pe_parser.hpp"
#include <algorithm>

namespace l4g::pe {

PEParser::PEParser(const uint8_t* base, size_t size) : base_(base), size_(size) {
    if (size < sizeof(IMAGE_DOS_HEADER)) return;
    const IMAGE_DOS_HEADER* dos = (const IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    if (dos->e_lfanew < 0 || (size_t)dos->e_lfanew + sizeof(IMAGE_NT_HEADERS) > size) return;
    
    nt_headers_ = (const IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt_headers_->Signature != IMAGE_NT_SIGNATURE) return;
    
    opt_header_ = &nt_headers_->OptionalHeader;
    is_64bit_ = (opt_header_->Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC);
    entry_point_ = opt_header_->AddressOfEntryPoint;
    image_base_ = is_64bit_ ? ((const IMAGE_OPTIONAL_HEADER64*)opt_header_)->ImageBase : opt_header_->ImageBase;
    image_size_ = opt_header_->SizeOfImage;
    valid_ = true;
}

PEParser::PEParser(const std::vector<uint8_t>& data) : PEParser(data.data(), data.size()) {}

std::vector<ExportEntry> PEParser::get_exports() const {
    std::vector<ExportEntry> exports;
    if (!valid_) return exports;
    
    auto export_dir = is_64bit_ 
        ? (const IMAGE_DATA_DIRECTORY*)&((const IMAGE_OPTIONAL_HEADER64*)opt_header_)->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT]
        : (const IMAGE_DATA_DIRECTORY*)&opt_header_->DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    
    if (export_dir->VirtualAddress == 0 || export_dir->Size == 0) return exports;
    
    const IMAGE_EXPORT_DIRECTORY* exp = (const IMAGE_EXPORT_DIRECTORY*)(base_ + export_dir->VirtualAddress);
    if ((uintptr_t)exp + sizeof(IMAGE_EXPORT_DIRECTORY) > (uintptr_t)base_ + size_) return exports;
    
    const uint32_t* names = (const uint32_t*)(base_ + exp->AddressOfNames);
    const uint32_t* funcs = (const uint32_t*)(base_ + exp->AddressOfFunctions);
    const uint16_t* ordinals = (const uint16_t*)(base_ + exp->AddressOfNameOrdinals);
    
    for (uint32_t i = 0; i < exp->NumberOfNames; ++i) {
        if (names[i] >= size_) continue;
        const char* name = (const char*)(base_ + names[i]);
        uintptr_t addr = (uintptr_t)(base_ + funcs[ordinals[i]]);
        exports.push_back({name, addr, (uint16_t)(ordinals[i] + exp->Base)});
    }
    return exports;
}

std::vector<ImportEntry> PEParser::get_imports() const {
    std::vector<ImportEntry> imports;
    if (!valid_) return imports;
    
    auto import_dir = is_64bit_ 
        ? (const IMAGE_DATA_DIRECTORY*)&((const IMAGE_OPTIONAL_HEADER64*)opt_header_)->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]
        : (const IMAGE_DATA_DIRECTORY*)&opt_header_->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    
    if (import_dir->VirtualAddress == 0) return imports;
    
    const IMAGE_IMPORT_DESCRIPTOR* imp = (const IMAGE_IMPORT_DESCRIPTOR*)(base_ + import_dir->VirtualAddress);
    while (imp->Name) {
        if (imp->Name >= size_) break;
        const char* dll_name = (const char*)(base_ + imp->Name);
        
        const uintptr_t* thunk = is_64bit_ 
            ? (const uintptr_t*)(base_ + imp->FirstThunk)
            : (const uintptr_t*)(base_ + imp->FirstThunk);
        const uintptr_t* orig_thunk = is_64bit_ 
            ? (const uintptr_t*)(base_ + imp->OriginalFirstThunk)
            : (const uintptr_t*)(base_ + imp->OriginalFirstThunk);
        
        for (size_t i = 0; thunk[i]; ++i) {
            uintptr_t thunk_val = thunk[i];
            uintptr_t orig_val = orig_thunk ? orig_thunk[i] : thunk_val;
            
            if (orig_val & (is_64bit_ ? 0x8000000000000000ULL : 0x80000000)) {
                // Import by ordinal
                imports.push_back({dll_name, "", (uintptr_t)&thunk[i], (uint16_t)(orig_val & 0xFFFF), true});
            } else {
                // Import by name
                if (orig_val + sizeof(WORD) < size_) {
                    const IMAGE_IMPORT_BY_NAME* ibn = (const IMAGE_IMPORT_BY_NAME*)(base_ + orig_val);
                    imports.push_back({dll_name, (const char*)ibn->Name, (uintptr_t)&thunk[i], ibn->Hint, false});
                }
            }
        }
        ++imp;
    }
    return imports;
}

std::vector<RelocationEntry> PEParser::get_relocations(uintptr_t delta) const {
    std::vector<RelocationEntry> relocs;
    if (!valid_ || delta == 0) return relocs;
    
    auto reloc_dir = is_64bit_ 
        ? (const IMAGE_DATA_DIRECTORY*)&((const IMAGE_OPTIONAL_HEADER64*)opt_header_)->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC]
        : (const IMAGE_DATA_DIRECTORY*)&opt_header_->DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    
    if (reloc_dir->VirtualAddress == 0) return relocs;
    
    const IMAGE_BASE_RELOCATION* reloc = (const IMAGE_BASE_RELOCATION*)(base_ + reloc_dir->VirtualAddress);
    while (reloc->VirtualAddress && reloc->SizeOfBlock >= sizeof(IMAGE_BASE_RELOCATION)) {
        size_t count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / sizeof(WORD);
        const WORD* entries = (const WORD*)((const uint8_t*)reloc + sizeof(IMAGE_BASE_RELOCATION));
        
        for (size_t i = 0; i < count; ++i) {
            uint16_t type = entries[i] >> 12;
            uint16_t offset = entries[i] & 0xFFF;
            if (type == IMAGE_REL_BASED_DIR64 || type == IMAGE_REL_BASED_HIGHLOW) {
                relocs.push_back({reloc->VirtualAddress + offset, type});
            }
        }
        reloc = (const IMAGE_BASE_RELOCATION*)((const uint8_t*)reloc + reloc->SizeOfBlock);
    }
    return relocs;
}

std::vector<SectionInfo> PEParser::get_sections() const {
    std::vector<SectionInfo> sections;
    if (!valid_) return sections;
    
    const IMAGE_SECTION_HEADER* sect = IMAGE_FIRST_SECTION(nt_headers_);
    for (int i = 0; i < nt_headers_->FileHeader.NumberOfSections; ++i) {
        sections.push_back({
            (const char*)sect[i].Name,
            sect[i].VirtualAddress,
            sect[i].Misc.VirtualSize,
            sect[i].SizeOfRawData,
            sect[i].PointerToRawData,
            sect[i].Characteristics
        });
    }
    return sections;
}

std::optional<uintptr_t> PEParser::get_export_address(const std::string& name) const {
    for (const auto& exp : get_exports()) {
        if (exp.name == name) return exp.address;
    }
    return std::nullopt;
}

std::optional<uintptr_t> PEParser::get_export_address(uint16_t ordinal) const {
    for (const auto& exp : get_exports()) {
        if (exp.ordinal == ordinal) return exp.address;
    }
    return std::nullopt;
}

void* PEParser::map_to_memory(HANDLE process) const {
    if (!valid_) return nullptr;
    
    void* base = VirtualAllocEx(process, (void*)image_base_, image_size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!base) {
        base = VirtualAllocEx(process, nullptr, image_size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!base) return nullptr;
    }
    
    // Write headers
    SIZE_T written = 0;
    WriteProcessMemory(process, base, base_, opt_header_->SizeOfHeaders, &written);
    
    // Write sections
    const IMAGE_SECTION_HEADER* sect = IMAGE_FIRST_SECTION(nt_headers_);
    for (int i = 0; i < nt_headers_->FileHeader.NumberOfSections; ++i) {
        if (sect[i].SizeOfRawData > 0) {
            void* dest = (uint8_t*)base + sect[i].VirtualAddress;
            WriteProcessMemory(process, dest, base_ + sect[i].PointerToRawData, sect[i].SizeOfRawData, &written);
        }
    }
    
    return base;
}

void PEParser::unmap_from_memory(void* base, HANDLE process) {
    if (base) VirtualFreeEx(process, base, 0, MEM_RELEASE);
}

} // namespace l4g::pe