#pragma once
#include <windows.h>

namespace l4g::cheat {

struct NoRecoilConfig {
    bool enabled = false;
    float recoil_x = 0.0f;
    float recoil_y = 0.0f;
    float recovery_speed = 10.0f;
};

class NoRecoil {
public:
    NoRecoil();
    ~NoRecoil();
    
    void set_config(const NoRecoilConfig& config);
    NoRecoilConfig get_config() const;
    
    void update(float* view_angles);
    
private:
    NoRecoilConfig config_;
    float last_punch_x_ = 0.0f;
    float last_punch_y_ = 0.0f;
};

} // namespace l4g::cheat