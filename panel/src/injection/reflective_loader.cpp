#include "reflective_loader.hpp"
#include <windows.h>
#include <vector>

namespace l4g::injection {

ReflectiveLoader::ReflectiveLoader(const std::vector<uint8_t>& dll_data) 
    : dll_data_(dll_data), parser_(dll_data.data(), dll_data.size()) {}

ReflectiveLoader::ReflectiveLoader(const uint8_t* data, size_t size) 
    : dll_data_(data, data + size), parser_(data, size) {}

ReflectiveLoadResult ReflectiveLoader::load(HANDLE target_process, void* param) {
    ReflectiveLoadResult result;
    
    if (!parser_.is_valid()) {
        result.error = "Invalid PE";
        return result;
    }
    
    // Allocate memory in target process
    void* base = parser_.map_to_memory(target_process);
    if (!base) {
        result.error = "Memory allocation failed";
        return result;
    }
    result.base_address = base;
    
    // Apply relocations
    uintptr_t delta = (uintptr_t)base - parser_.get_image_base();
    if (delta != 0) {
        if (!apply_relocations(target_process, base, parser_, delta)) {
            result.error = "Relocation failed";
            parser_.unmap_from_memory(base, target_process);
            return result;
        }
    }
    
    // Resolve imports
    if (!resolve_imports(target_process, base, parser_)) {
        result.error = "Import resolution failed";
        parser_.unmap_from_memory(base, target_process);
        return result;
    }
    
    // Protect sections
    if (!protect_sections(target_process, base, parser_)) {
        result.error = "Section protection failed";
        parser_.unmap_from_memory(base, target_process);
        return result;
    }
    
    // Call entry point
    if (!call_entry_point(target_process, base, parser_, param, result.thread_handle, result.thread_id)) {
        result.error = "Entry point call failed";
        parser_.unmap_from_memory(base, target_process);
        return result;
    }
    
    result.success = true;
    result.entry_point = parser_.get_entry_point();
    return result;
}

ReflectiveLoadResult ReflectiveLoader::load_from_memory(HANDLE target_process, const std::vector<uint8_t>& dll_data, void* param) {
    ReflectiveLoader loader(dll_data);
    return loader.load(target_process, param);
}

bool ReflectiveLoader::map_sections(HANDLE process, void* base, const pe::PEParser& parser) {
    // Already done in map_to_memory
    return base != nullptr;
}

bool ReflectiveLoader::apply_relocations(HANDLE process, void* base, const pe::PEParser& parser, uintptr_t delta) {
    auto relocs = parser.get_relocations(delta);
    for (const auto& reloc : relocs) {
        uintptr_t addr = (uintptr_t)base + reloc.address;
        if (reloc.type == IMAGE_REL_BASED_DIR64) {
            uint64_t value = 0;
            SIZE_T read = 0;
            if (!ReadProcessMemory(process, (void*)addr, &value, 8, &read) || read != 8) return false;
            value += delta;
            SIZE_T written = 0;
            if (!WriteProcessMemory(process, (void*)addr, &value, 8, &written) || written != 8) return false;
        } else if (reloc.type == IMAGE_REL_BASED_HIGHLOW) {
            uint32_t value = 0;
            SIZE_T read = 0;
            if (!ReadProcessMemory(process, (void*)addr, &value, 4, &read) || read != 4) return false;
            value += (uint32_t)delta;
            SIZE_T written = 0;
            if (!WriteProcessMemory(process, (void*)addr, &value, 4, &written) || written != 4) return false;
        }
    }
    return true;
}

bool ReflectiveLoader::resolve_imports(HANDLE process, void* base, const pe::PEParser& parser) {
    auto imports = parser.get_imports();
    for (const auto& imp : imports) {
        HMODULE hMod = LoadLibraryA(imp.dll_name.c_str());
        if (!hMod) return false;
        
        void* func_addr = nullptr;
        if (imp.is_ordinal) {
            func_addr = GetProcAddress(hMod, (LPCSTR)imp.ordinal);
        } else {
            func_addr = GetProcAddress(hMod, imp.function_name.c_str());
        }
        if (!func_addr) return false;
        
        SIZE_T written = 0;
        if (!WriteProcessMemory(process, (void*)imp.iat_address, &func_addr, sizeof(void*), &written) || written != sizeof(void*)) {
            return false;
        }
    }
    return true;
}

bool ReflectiveLoader::protect_sections(HANDLE process, void* base, const pe::PEParser& parser) {
    auto sections = parser.get_sections();
    for (const auto& sect : sections) {
        DWORD protect = PAGE_READONLY;
        if (sect.characteristics & IMAGE_SCN_MEM_EXECUTE) protect = PAGE_EXECUTE_READ;
        if (sect.characteristics & IMAGE_SCN_MEM_WRITE) {
            if (protect == PAGE_EXECUTE_READ) protect = PAGE_EXECUTE_READWRITE;
            else protect = PAGE_READWRITE;
        }
        if (sect.characteristics & IMAGE_SCN_MEM_NOT_CACHED) protect |= PAGE_NOCACHE;
        
        DWORD old_protect = 0;
        void* addr = (uint8_t*)base + sect.virtual_address;
        SIZE_T size = sect.virtual_size;
        if (!VirtualProtectEx(process, addr, size, protect, &old_protect)) return false;
    }
    return true;
}

bool ReflectiveLoader::call_entry_point(HANDLE process, void* base, const pe::PEParser& parser, void* param, HANDLE& out_thread, DWORD& out_tid) {
    using DllEntry_t = BOOL(WINAPI*)(void*, DWORD, void*);
    DllEntry_t entry = (DllEntry_t)((uint8_t*)base + parser.get_entry_point());
    
    // Create remote thread to call DllMain(DLL_PROCESS_ATTACH)
    out_thread = CreateRemoteThread(process, nullptr, 0, (LPTHREAD_START_ROUTINE)entry, param, 0, &out_tid);
    return out_thread != nullptr;
}

} // namespace l4g::injection