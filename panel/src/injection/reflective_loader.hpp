#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <optional>
#include "../utils/pe_parser.hpp"

namespace l4g::injection {

struct ReflectiveLoadResult {
    bool success = false;
    void* base_address = nullptr;
    uintptr_t entry_point = 0;
    HANDLE thread_handle = nullptr;
    DWORD thread_id = 0;
    std::string error;
};

class ReflectiveLoader {
public:
    explicit ReflectiveLoader(const std::vector<uint8_t>& dll_data);
    explicit ReflectiveLoader(const uint8_t* data, size_t size);
    
    ReflectiveLoadResult load(HANDLE target_process = GetCurrentProcess(), void* param = nullptr);
    static ReflectiveLoadResult load_from_memory(HANDLE target_process, const std::vector<uint8_t>& dll_data, void* param = nullptr);
    
    // In-memory PE mapping without disk write
    static bool map_sections(HANDLE process, void* base, const pe::PEParser& parser);
    static bool apply_relocations(HANDLE process, void* base, const pe::PEParser& parser, uintptr_t delta);
    static bool resolve_imports(HANDLE process, void* base, const pe::PEParser& parser);
    static bool protect_sections(HANDLE process, void* base, const pe::PEParser& parser);
    static bool call_entry_point(HANDLE process, void* base, const pe::PEParser& parser, void* param, HANDLE& out_thread, DWORD& out_tid);
    
private:
    std::vector<uint8_t> dll_data_;
    pe::PEParser parser_;
};

} // namespace l4g::injection