#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>

namespace l4g::evasion {

class CacheWiper {
public:
    CacheWiper();
    ~CacheWiper();
    
    void start_monitoring();
    void stop_monitoring();
    
    // Wipe various caches
    bool wipe_prefetch(const wchar_t* exe_name = nullptr);
    bool wipe_amcache(const wchar_t* exe_name = nullptr);
    bool wipe_srum();
    bool wipe_shimcache();
    bool wipe_usn_journal();
    bool wipe_thumbnail_cache();
    bool wipe_all_caches();
    
    // Wipe ESP overlay cache on write
    bool wipe_on_write(const wchar_t* file_path);
    
    // Continuous monitoring
    void monitor_loop();
    
private:
    std::thread monitor_thread_;
    std::atomic<bool> running_{false};
    
    bool delete_prefetch_files(const wchar_t* pattern);
    bool delete_amcache_entries(const wchar_t* exe_name);
    bool clear_srum_database();
    bool clear_shimcache();
    bool disable_usn_journal();
};

} // namespace l4g::evasion