#pragma once
#include <windows.h>

namespace l4g::cheat {

struct SpeedHackConfig {
    bool enabled = false;
    float speed = 1.0f;
    int toggle_key = 0x70; // F1
};

class SpeedHack {
public:
    SpeedHack();
    ~SpeedHack();
    
    void set_config(const SpeedHackConfig& config);
    SpeedHackConfig get_config() const;
    
    void update();
    void toggle();
    
private:
    SpeedHackConfig config_;
    bool last_key_state_ = false;
    
    // Hook targets
    static bool hook_QueryPerformanceCounter();
    static bool hook_GetTickCount64();
    static bool hook_timeGetTime();
};

} // namespace l4g::cheat