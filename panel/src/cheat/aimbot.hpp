#pragma once
#include <windows.h>
#include <vector>
#include <mutex>
#include "types.hpp"

namespace l4g::cheat {

struct AimbotConfig {
    bool enabled = false;
    bool visible_only = true;
    float fov = 5.0f;
    float smooth = 3.0f;
    int target_bone = 0; // 0=head, 1=neck, 2=chest
    int trigger_key = 0x01; // VK_LBUTTON
    float max_distance = 200.0f;
    bool ignore_teammates = true;
    bool ignore_knocked = true;
};

class Aimbot {
public:
    Aimbot();
    ~Aimbot();
    
    void set_config(const AimbotConfig& config);
    AimbotConfig get_config() const;
    
    void update(const std::vector<uint64_t>& entity_list, const Vec3& local_pos, const Vec3& local_view_angles);
    void aim_at(const Vec3& target_pos);
    
    static Vec3 calculate_angle(const Vec3& from, const Vec3& to);
    static Vec2 world_to_screen(const Vec3& world, const float* view_matrix, int width, int height);
    static float get_fov(const Vec3& view_angles, const Vec3& target_angle);
    
private:
    AimbotConfig config_;
    mutable std::mutex mutex_;
    
    uint64_t find_best_target(const std::vector<uint64_t>& entities, const Vec3& local_pos, const Vec3& view_angles);
    bool is_visible(uint64_t entity);
    Vec3 get_bone_pos(uint64_t entity, int bone_id);
    void smooth_aim(Vec3& current, const Vec3& target);
};

} // namespace l4g::cheat