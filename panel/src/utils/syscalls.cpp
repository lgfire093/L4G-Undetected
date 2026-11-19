#include "syscalls.hpp"
#include <windows.h>
#include <winternl.h>

namespace l4g::syscalls {

static SyscallTable g_table;

SyscallTable& get_syscall_table() {
    return g_table;
}

void* get_syscall_address(const char* function_name) {
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll) return nullptr;
    return GetProcAddress(ntdll, function_name);
}

uint16_t get_syscall_number(const char* function_name) {
    void* addr = get_syscall_address(function_name);
    if (!addr) return 0;
    
    // Parse syscall number from: mov r10, rcx; mov eax, <syscall_num>; syscall
    uint8_t* bytes = (uint8_t*)addr;
    if (bytes[0] == 0x4C && bytes[1] == 0x8B && bytes[2] == 0xD1 && // mov r10, rcx
        bytes[3] == 0xB8) { // mov eax, imm32
        return *(uint32_t*)(bytes + 4) & 0xFFFF;
    }
    return 0;
}

bool initialize_syscalls() {
    g_table.NtQuerySystemInformation = (NtQuerySystemInformation_t)get_syscall_address("NtQuerySystemInformation");
    g_table.NtCreateThreadEx = (NtCreateThreadEx_t)get_syscall_address("NtCreateThreadEx");
    g_table.NtWriteVirtualMemory = (NtWriteVirtualMemory_t)get_syscall_address("NtWriteVirtualMemory");
    g_table.NtProtectVirtualMemory = (NtProtectVirtualMemory_t)get_syscall_address("NtProtectVirtualMemory");
    g_table.NtAllocateVirtualMemory = (NtAllocateVirtualMemory_t)get_syscall_address("NtAllocateVirtualMemory");
    g_table.NtFreeVirtualMemory = (NtFreeVirtualMemory_t)get_syscall_address("NtFreeVirtualMemory");
    g_table.NtOpenProcess = (NtOpenProcess_t)get_syscall_address("NtOpenProcess");
    g_table.NtTerminateProcess = (NtTerminateProcess_t)get_syscall_address("NtTerminateProcess");
    g_table.NtQueryInformationProcess = (NtQueryInformationProcess_t)get_syscall_address("NtQueryInformationProcess");
    g_table.NtSetInformationProcess = (NtSetInformationProcess_t)get_syscall_address("NtSetInformationProcess");
    g_table.NtCreateSection = (NtCreateSection_t)get_syscall_address("NtCreateSection");
    g_table.NtMapViewOfSection = (NtMapViewOfSection_t)get_syscall_address("NtMapViewOfSection");
    g_table.NtUnmapViewOfSection = (NtUnmapViewOfSection_t)get_syscall_address("NtUnmapViewOfSection");
    g_table.NtCreateFile = (NtCreateFile_t)get_syscall_address("NtCreateFile");
    g_table.NtDeviceIoControlFile = (NtDeviceIoControlFile_t)get_syscall_address("NtDeviceIoControlFile");
    g_table.NtLoadDriver = (NtLoadDriver_t)get_syscall_address("NtLoadDriver");
    g_table.NtUnloadDriver = (NtUnloadDriver_t)get_syscall_address("NtUnloadDriver");
    g_table.NtCreateKey = (NtCreateKey_t)get_syscall_address("NtCreateKey");
    g_table.NtSetValueKey = (NtSetValueKey_t)get_syscall_address("NtSetValueKey");
    g_table.NtDeleteKey = (NtDeleteKey_t)get_syscall_address("NtDeleteKey");
    g_table.NtQueryDirectoryFile = (NtQueryDirectoryFile_t)get_syscall_address("NtQueryDirectoryFile");
    
    return g_table.NtQuerySystemInformation != nullptr;
}

} // namespace l4g::syscalls