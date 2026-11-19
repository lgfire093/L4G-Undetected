#pragma once
#include <windows.h>
#include <winternl.h>

namespace l4g::syscalls {

// Direct syscall stubs - resolved at runtime from ntdll
using NtQuerySystemInformation_t = NTSTATUS(NTAPI*)(SYSTEM_INFORMATION_CLASS, PVOID, ULONG, PULONG);
using NtCreateThreadEx_t = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, HANDLE, PVOID, PVOID, ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);
using NtWriteVirtualMemory_t = NTSTATUS(NTAPI*)(HANDLE, PVOID, PVOID, SIZE_T, PSIZE_T);
using NtProtectVirtualMemory_t = NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG, PULONG);
using NtAllocateVirtualMemory_t = NTSTATUS(NTAPI*)(HANDLE, PVOID*, ULONG_PTR, PSIZE_T, ULONG, ULONG);
using NtFreeVirtualMemory_t = NTSTATUS(NTAPI*)(HANDLE, PVOID*, PSIZE_T, ULONG);
using NtOpenProcess_t = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PCLIENT_ID);
using NtTerminateProcess_t = NTSTATUS(NTAPI*)(HANDLE, NTSTATUS);
using NtQueryInformationProcess_t = NTSTATUS(NTAPI*)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG, PULONG);
using NtSetInformationProcess_t = NTSTATUS(NTAPI*)(HANDLE, PROCESSINFOCLASS, PVOID, ULONG);
using NtCreateSection_t = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PLARGE_INTEGER, ULONG, ULONG, HANDLE);
using NtMapViewOfSection_t = NTSTATUS(NTAPI*)(HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER, PSIZE_T, SECTION_INHERIT, ULONG, ULONG);
using NtUnmapViewOfSection_t = NTSTATUS(NTAPI*)(HANDLE, PVOID);
using NtCreateFile_t = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, PIO_STATUS_BLOCK, PLARGE_INTEGER, ULONG, ULONG, ULONG, ULONG, PVOID, ULONG);
using NtDeviceIoControlFile_t = NTSTATUS(NTAPI*)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK, ULONG, PVOID, ULONG, PVOID, ULONG);
using NtLoadDriver_t = NTSTATUS(NTAPI*)(PUNICODE_STRING);
using NtUnloadDriver_t = NTSTATUS(NTAPI*)(PUNICODE_STRING);
using NtCreateKey_t = NTSTATUS(NTAPI*)(PHANDLE, ACCESS_MASK, POBJECT_ATTRIBUTES, ULONG, PUNICODE_STRING, ULONG, PULONG);
using NtSetValueKey_t = NTSTATUS(NTAPI*)(HANDLE, PUNICODE_STRING, ULONG, ULONG, PVOID, ULONG);
using NtDeleteKey_t = NTSTATUS(NTAPI*)(HANDLE);
using NtQueryDirectoryFile_t = NTSTATUS(NTAPI*)(HANDLE, HANDLE, PVOID, PVOID, PIO_STATUS_BLOCK, PVOID, ULONG, FILE_INFORMATION_CLASS, BOOLEAN, PUNICODE_STRING, BOOLEAN);

struct SyscallTable {
    NtQuerySystemInformation_t NtQuerySystemInformation = nullptr;
    NtCreateThreadEx_t NtCreateThreadEx = nullptr;
    NtWriteVirtualMemory_t NtWriteVirtualMemory = nullptr;
    NtProtectVirtualMemory_t NtProtectVirtualMemory = nullptr;
    NtAllocateVirtualMemory_t NtAllocateVirtualMemory = nullptr;
    NtFreeVirtualMemory_t NtFreeVirtualMemory = nullptr;
    NtOpenProcess_t NtOpenProcess = nullptr;
    NtTerminateProcess_t NtTerminateProcess = nullptr;
    NtQueryInformationProcess_t NtQueryInformationProcess = nullptr;
    NtSetInformationProcess_t NtSetInformationProcess = nullptr;
    NtCreateSection_t NtCreateSection = nullptr;
    NtMapViewOfSection_t NtMapViewOfSection = nullptr;
    NtUnmapViewOfSection_t NtUnmapViewOfSection = nullptr;
    NtCreateFile_t NtCreateFile = nullptr;
    NtDeviceIoControlFile_t NtDeviceIoControlFile = nullptr;
    NtLoadDriver_t NtLoadDriver = nullptr;
    NtUnloadDriver_t NtUnloadDriver = nullptr;
    NtCreateKey_t NtCreateKey = nullptr;
    NtSetValueKey_t NtSetValueKey = nullptr;
    NtDeleteKey_t NtDeleteKey = nullptr;
    NtQueryDirectoryFile_t NtQueryDirectoryFile = nullptr;
};

SyscallTable& get_syscall_table();
bool initialize_syscalls();
void* get_syscall_address(const char* function_name);
uint16_t get_syscall_number(const char* function_name);

} // namespace l4g::syscalls