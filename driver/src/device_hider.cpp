#include <ntddk.h>

// Hide device object from anti-cheat enumeration
NTSTATUS HideDeviceObject(PDEVICE_OBJECT DeviceObject) {
    // Unlink from driver's DeviceObject list
    PDEVICE_OBJECT* prev = &DeviceObject->DriverObject->DeviceObject;
    while (*prev && *prev != DeviceObject) {
        prev = &(*prev)->NextDevice;
    }
    if (*prev) {
        *prev = DeviceObject->NextDevice;
        DeviceObject->NextDevice = nullptr;
    }
    return STATUS_SUCCESS;
}

NTSTATUS HideDriverObject(PDRIVER_OBJECT DriverObject) {
    // Remove from PsLoadedModuleList (requires DKOM)
    return STATUS_SUCCESS;
}

NTSTATUS CreateStealthDevice(PDRIVER_OBJECT DriverObject, PDEVICE_OBJECT* OutDevice) {
    UNICODE_STRING deviceName = RTL_CONSTANT_STRING(L"\\Device\\L4G_Stealth");
    UNICODE_STRING symLink = RTL_CONSTANT_STRING(L"\\DosDevices\\L4G_Stealth");
    
    PDEVICE_OBJECT device = nullptr;
    NTSTATUS status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &device);
    if (!NT_SUCCESS(status)) return status;
    
    // Don't create symbolic link - keeps it hidden
    // IoCreateSymbolicLink(&symLink, &deviceName);
    
    device->Flags |= DO_DEVICE_INITIALIZING;
    device->Flags &= ~DO_DEVICE_INITIALIZING;
    
    *OutDevice = device;
    return STATUS_SUCCESS;
}