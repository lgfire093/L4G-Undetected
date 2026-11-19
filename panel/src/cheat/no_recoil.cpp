#include "no_recoil.hpp"

namespace l4g::cheat {

NoRecoil::NoRecoil() = default;
NoRecoil::~NoRecoil() = default;

void NoRecoil::set_config(const NoRecoilConfig& config) {
    config_ = config;
}

NoRecoilConfig NoRecoil::get_config() const {
    return config_;
}

void NoRecoil::update(float* view_angles) {
    if (!config_.enabled || !view_angles) return;
    
    // Read current punch angle from game memory
    float punch_x = 0.0f, punch_y = 0.0f;
    // punch_x = read_memory(local_player + OFFSET_AIM_PUNCH_X);
    // punch_y = read_memory(local_player + OFFSET_AIM_PUNCH_Y);
    
    // Compensate
    view_angles[0] -= (punch_x - last_punch_x_) * config_.recovery_speed;
    view_angles[1] -= (punch_y - last_punch_y_) * config_.recovery_speed;
    
    last_punch_x_ = punch_x;
    last_punch_y_ = punch_y;
}

} // namespace l4g::cheat