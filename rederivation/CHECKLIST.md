# L4G AIMS - Per-Build Re-Derivation Checklist

This document describes the workflow to re-derive all offsets, syscalls, and addresses for each new game build and Windows build.

## 1. GAME OFFSETS (Free Fire / Free Fire Max)

### 1.1 Required Offsets
| Offset Name | Description | Hunt Method |
|-------------|-------------|-------------|
| `InitBase` | Global game base pointer | Scan for `GameAssembly.dll` + known pattern |
| `AvatarManager` | Avatar manager singleton | Cross-ref `AvatarManager::Instance` |
| `FollowCamera` | Camera follow pointer | Cross-ref `FollowCamera::get_Main` |
| `Player_Name` | Player name string offset | Search UTF-16 "Player" in heap |
| `Bone_0` (Head) | Head bone index | Search bone array for head position |
| `Bone_11` (Root) | Root/pelvis bone index | Search bone array for pelvis position |
| `Bone_8` (Spine) | Spine bone index | Search bone array for chest position |
| `Bone_12` (RShoulder) | Right shoulder bone | Search bone array |
| `Bone_4` (LElbow) | Left elbow bone | Search bone array |
| `Bone_14` (RElbow) | Right elbow bone | Search bone array |
| `ViewMatrix` | World-to-screen matrix | Cross-ref `Camera::WorldToScreenPoint` |
| `LocalPlayer` | Local player pointer | Cross-ref `Player::get_LocalPlayer` |
| `EntityList` | Entity dictionary | Cross-ref `GameManager::Entities` |

### 1.2 Hunt Process
1. **Dump GameAssembly.dll** from target game version
2. **Load in IDA Pro / Ghidra** with Unity metadata
3. **Find Il2Cpp classes**: `Player`, `Avatar`, `Camera`, `Bone`, `GameManager`
4. **Extract field offsets** from `Il2CppClass` field list
5. **Verify static pointers** by scanning for known patterns
6. **Test in-game** with memory reader
7. **Update** `OFFSETANDBONES.txt` and `free fire max Offsets.cs`

### 1.3 Automation Script
```python
# rederivation/scripts/hunt_game_offsets.py
# Usage: python hunt_game_offsets.py --gameassembly GameAssembly.dll --metadata global-metadata.dat
```

## 2. WINDOWS SYSCALLS (Windows 11)

### 2.1 Required Syscalls
| Syscall | Purpose | Hunt Method |
|---------|---------|-------------|
| `NtQuerySystemInformation` | Process/module enumeration | Parse ntdll export |
| `NtCreateThreadEx` | Thread creation | Parse ntdll export |
| `NtWriteVirtualMemory` | Memory write | Parse ntdll export |
| `NtProtectVirtualMemory` | Memory protection | Parse ntdll export |
| `NtAllocateVirtualMemory` | Memory allocation | Parse ntdll export |
| `NtOpenProcess` | Process handle | Parse ntdll export |
| `NtTerminateProcess` | Process kill | Parse ntdll export |
| `NtCreateSection` | Section creation | Parse ntdll export |
| `NtMapViewOfSection` | Section mapping | Parse ntdll export |
| `NtLoadDriver` | Driver loading | Parse ntdll export |

### 2.2 Hunt Process
1. **Dump ntdll.dll** from target Windows build
2. **Parse export table** for syscall stubs
3. **Extract syscall numbers** from `mov eax, <num>` instruction
4. **Verify** by calling via direct syscall
5. **Update** `syscalls.cpp` with new numbers

### 2.3 Automation Script
```python
# rederivation/scripts/hunt_syscalls.py
# Usage: python hunt_syscalls.py --ntdll C:\Windows\System32\ntdll.dll
```

## 3. KERNEL NOTIFY ROUTINES

### 3.1 Required Addresses
| Array | Purpose | Hunt Method |
|-------|---------|-------------|
| `PspCreateProcessNotifyRoutine` | Process creation callbacks | Scan kernel for pattern |
| `PspCreateThreadNotifyRoutine` | Thread creation callbacks | Scan kernel for pattern |
| `PspLoadImageNotifyRoutine` | Module load callbacks | Scan kernel for pattern |
| `PspNotifyEnableMask` | Notify routine bitmask | Cross-ref from arrays |

### 3.2 Hunt Process
1. **Load ntoskrnl.exe** in IDA Pro
2. **Search for patterns**:
   - `PspCreateProcessNotifyRoutine`: `48 8D 0D ? ? ? ? 45 33 C0 48 8B D7`
   - `PspCreateThreadNotifyRoutine`: Similar pattern
   - `PspLoadImageNotifyRoutine`: Similar pattern
3. **Calculate offsets** from kernel base
4. **Update** `ac_neutralizer.cpp` with new addresses

### 3.3 Automation Script
```python
# rederivation/scripts/hunt_notify.py
# Usage: python hunt_notify.py --ntoskrnl C:\Windows\System32\ntoskrnl.exe
```

## 4. ANTI-CHEAT DRIVER DISPATCH ROUTINES

### 4.1 Target Drivers
| Driver | Device Name | Dispatch Offset Hunt |
|--------|-------------|---------------------|
| BEDaisy.sys | \Device\BEDaisy | MajorFunction[IRP_MJ_DEVICE_CONTROL] |
| BEKernel.sys | \Device\BEKernel | MajorFunction[IRP_MJ_DEVICE_CONTROL] |
| EasyAntiCheat.sys | \Device\EasyAntiCheat | MajorFunction[IRP_MJ_DEVICE_CONTROL] |
| detectac.sys | \Device\detectac | MajorFunction[IRP_MJ_DEVICE_CONTROL] |

### 4.2 Hunt Process
1. **Load driver in IDA Pro**
2. **Find DriverEntry** → get `DriverObject`
3. **Locate MajorFunction array** initialization
4. **Extract IRP_MJ_DEVICE_CONTROL handler** (index 0x0E)
5. **Update** `ac_neutralizer.cpp` with handler addresses

### 4.3 Automation Script
```python
# rederivation/scripts/hunt_dispatch.py
# Usage: python hunt_dispatch.py --driver BEDaisy.sys
```

## 5. VULNERABLE OEM DRIVER IOCTLS

### 5.1 Required IOCTLs
| Driver | Write IOCTL | Read IOCTL | Map IOCTL | Hunt Method |
|--------|-------------|------------|-----------|-------------|
| IntelHDAudio.sys | 0x80002040 | 0x80002044 | 0x80002048 | Reverse IRP_MJ_DEVICE_CONTROL |
| RTCore64.sys | 0x80102040 | 0x80102044 | - | Reverse IRP_MJ_DEVICE_CONTROL |
| AsIO.sys | 0xA040244C | 0xA0402448 | - | Reverse IRP_MJ_DEVICE_CONTROL |

### 5.2 Hunt Process
1. **Load driver in IDA Pro**
2. **Find DeviceControl dispatch** routine
3. **Analyze IOCTL switch/case** statements
4. **Identify** arbitrary read/write/map IOCTLs
5. **Extract** input buffer structure offsets
6. **Update** `byovd.cpp` with new IOCTLs

### 5.3 Automation Script
```python
# rederivation/scripts/hunt_ioctls.py
# Usage: python hunt_ioctls.py --driver IntelHDAudio.sys
```

## 6. DKOM OFFSETS (EPROCESS/ETHREAD)

### 6.1 Required Offsets (Windows 11)
| Structure | Field | Offset (22H2) | Offset (23H2) | Hunt Method |
|-----------|-------|---------------|---------------|-------------|
| EPROCESS | ActiveProcessLinks | 0x448 | 0x450 | `dt nt!_EPROCESS` |
| EPROCESS | UniqueProcessId | 0x440 | 0x448 | `dt nt!_EPROCESS` |
| EPROCESS | ImageFileName | 0x5A8 | 0x5B0 | `dt nt!_EPROCESS` |
| EPROCESS | Token | 0x4B8 | 0x4C0 | `dt nt!_EPROCESS` |
| ETHREAD | ThreadListEntry | 0x4E8 | 0x4F0 | `dt nt!_ETHREAD` |
| ETHREAD | ClientId | 0x6D0 | 0x6D8 | `dt nt!_ETHREAD` |
| LDR_DATA_TABLE_ENTRY | InLoadOrderLinks | 0x30 | 0x30 | `dt nt!_LDR_DATA_TABLE_ENTRY` |
| LDR_DATA_TABLE_ENTRY | BaseDllName | 0x58 | 0x60 | `dt nt!_LDR_DATA_TABLE_ENTRY` |
| LDR_DATA_TABLE_ENTRY | DllBase | 0x30 | 0x30 | `dt nt!_LDR_DATA_TABLE_ENTRY` |

### 6.2 Hunt Process
1. **Attach kernel debugger** (WinDbg/KD)
2. **Run `dt nt!_EPROCESS`**, `dt nt!_ETHREAD`, `dt nt!_LDR_DATA_TABLE_ENTRY`
3. **Record offsets** for current build
4. **Find PsActiveProcessHead**: `x nt!PsActiveProcessHead`
5. **Find PsLoadedModuleList**: `x nt!PsLoadedModuleList`
6. **Update** `dkom.cpp` with new offsets

### 6.3 Automation Script
```python
# rederivation/scripts/hunt_dkom.py
# Usage: python hunt_dkom.py --windbg-script hunt_dkom.txt
```

## 7. CI/CD INTEGRATION

### 7.1 Build Pipeline
```yaml
# .github/workflows/rederive.yml
name: Re-Derive Offsets
on:
  schedule:
    - cron: '0 0 * * 0'  # Weekly
  workflow_dispatch:
    inputs:
      game_version:
        description: 'Game version to target'
        required: true
      windows_build:
        description: 'Windows build number'
        required: true

jobs:
  rederive:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - name: Download GameAssembly
        run: scripts/download_gameassembly.py ${{ inputs.game_version }}
      - name: Hunt Game Offsets
        run: python rederivation/scripts/hunt_game_offsets.py
      - name: Hunt Syscalls
        run: python rederivation/scripts/hunt_syscalls.py
      - name: Hunt Notify Routines
        run: python rederivation/scripts/hunt_notify.py
      - name: Hunt Dispatch Routines
        run: python rederivation/scripts/hunt_dispatch.py
      - name: Hunt IOCTLs
        run: python rederivation/scripts/hunt_ioctls.py
      - name: Hunt DKOM Offsets
        run: python rederivation/scripts/hunt_dkom.py
      - name: Update Source Files
        run: scripts/update_offsets.py
      - name: Build & Test
        run: cmake --build build --target L4G_Panel
      - name: Create PR
        uses: peter-evans/create-pull-request@v5
```

## 8. VERIFICATION CHECKLIST

Before releasing new build:
- [ ] Game offsets tested in-game (ESP renders correctly)
- [ ] Syscalls work (direct syscall execution succeeds)
- [ ] BYOVD finds vulnerable driver (arbitrary read/write works)
- [ ] DKOM hides process (Task Manager doesn't show it)
- [ ] AC neutralizer removes notify routines (no callbacks fire)
- [ ] DoH beacon works (C2 receives heartbeat)
- [ ] APK connects to panel (commands execute)
- [ ] Auto-update works (panel downloads new version)
- [ ] Persistence survives reboot (service/WMI starts panel)
- [ ] Log wiper clears traces (Event Viewer empty)
- [ ] Cache wiper cleans prefetch/AmCache
- [ ] Fingerprint spoofer changes HWID

## 9. ROLLBACK PROCEDURE

If new offsets break functionality:
1. Revert `OFFSETANDBONES.txt` to previous version
2. Revert `syscalls.cpp` syscall numbers
3. Revert kernel offsets in `dkom.cpp`, `ac_neutralizer.cpp`
4. Rebuild and test
5. Document failure in build log