#pragma once
#include <windows.h>
#include <winternl.h>

// windows.h declares the STATUS_* codes but not the NT_SUCCESS macro; the
// SDK headers (bcrypt.h, ntifs.h) only define it in kernel-mode contexts.
#ifndef NT_SUCCESS
#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)
#endif

// winternl.h stops at SYSTEM_INFORMATION_CLASS; the module enumeration struct
// is otherwise only in the WDK. Layout matches the documented consumer shape
// (w2k3 .. win11) so a user-mode NtQuerySystemInformation walk works as-is.
typedef struct _L4G_RTL_PROCESS_MODULE_INFORMATION {
    PVOID Section;
    PVOID MappedBase;
    PVOID ImageBase;
    ULONG ImageSize;
    ULONG Flags;
    USHORT LoadOrderIndex;
    USHORT InitOrderIndex;
    USHORT LoadCount;
    USHORT OffsetToFileName;
    UCHAR FullPathName[256];
} L4G_RTL_PROCESS_MODULE_INFORMATION, *PL4G_RTL_PROCESS_MODULE_INFORMATION;

typedef struct _L4G_RTL_PROCESS_MODULES {
    ULONG NumberOfModules;
    L4G_RTL_PROCESS_MODULE_INFORMATION Modules[1];
} L4G_RTL_PROCESS_MODULES, *PL4G_RTL_PROCESS_MODULES;
