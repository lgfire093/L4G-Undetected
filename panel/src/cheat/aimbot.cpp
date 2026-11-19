#include "aimbot.hpp"
#include <cmath>
#include <algorithm>

namespace l4g::cheat {

Aimbot::Aimbot() = default;
Aimbot::~Aimbot() = default;

void Aimbot::set_config(const AimbotConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

AimbotConfig Aimbot::get_config() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void Aimbot::update(const std::vector<uint64_t>& entity_list, const Vec3& local_pos, const Vec3& local_view_angles) {
    if (!config_.enabled) return;
    
    // Check trigger key
    if (!(GetAsyncKeyState(config_.trigger_key) & 0x8000)) return;
    
    uint64_t target = find_best_target(entity_list, local_pos, local_view_angles);
    if (target) {
        Vec3 target_pos = get_bone_pos(target, config_.target_bone);
        aim_at(target_pos);
    }
}

uint64_t Aimbot::find_best_target(const std::vector<uint64_t>& entities, const Vec3& local_pos, const Vec3& view_angles) {
    uint64_t best = 0;
    float best_fov = config_.fov;
    
    for (uint64_t entity : entities) {
        // Check distance
        Vec3 entity_pos = get_bone_pos(entity, 0); // root
        float dx = entity_pos.x - local_pos.x;
        float dy = entity_pos.y - local_pos.y;
        float dz = entity_pos.z - local_pos.z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        
        if (dist > config_.max_distance) continue;
        
        // Check team
        if (config_.ignore_teammates) {
            // Read team from entity - placeholder
        }
        
        // Check knocked
        if (config_.ignore_knocked) {
            // Read knocked state - placeholder
        }
        
        // Check visibility
        if (config_.visible_only && !is_visible(entity)) continue;
        
        // Calculate FOV
        Vec3 target_pos = get_bone_pos(entity, config_.target_bone);
        Vec3 angle = calculate_angle(local_pos, target_pos);
        float fov = get_fov(view_angles, angle);
        
        if (fov < best_fov) {
            best_fov = fov;
            best = entity;
        }
    }
    
    return best;
}

bool Aimbot::is_visible(uint64_t entity) {
    // Ray trace from local player to target
    // Placeholder - would need game-specific implementation
    return true;
}

Vec3 Aimbot::get_bone_pos(uint64_t entity, int bone_id) {
    // Read bone matrix from entity
    // Placeholder
    return {0, 0, 0};
}

void Aimbot::aim_at(const Vec3& target_pos) {
    // Send mouse input or write to game memory
    // Placeholder
}

Vec3 Aimbot::calculate_angle(const Vec3& from, const Vec3& to) {
    Vec3 delta = {to.x - from.x, to.y - from.y, to.z - from.z};
    float hyp = sqrtf(delta.x * delta.x + delta.y * delta.y);
    Vec3 angle;
    angle.x = -asinf(delta.z / hyp) * 180.0f / 3.14159265f;
    angle.y = atan2f(delta.y, delta.x) * 180.0f / 3.14159265f;
    angle.z = 0.0f;
    return angle;
}

Vec2 Aimbot::world_to_screen(const Vec3& world, const float* view_matrix, int width, int height) {
    Vec2 screen = {0, 0};
    if (!view_matrix) return screen;
    
    float w = view_matrix[12] * world.x + view_matrix[13] * world.y + view_matrix[14] * world.z + view_matrix[15];
    if (w < 0.01f) return screen;
    
    float x = view_matrix[0] * world.x + view_matrix[1] * world.y + view_matrix[2] * world.z + view_matrix[3];
    float y = view_matrix[4] * world.x + view_matrix[5] * world.y + view_matrix[6] * world.z + view_matrix[7];
    
    screen.x = (width / 2.0f) * (1.0f + x / w);
    screen.y = (height / 2.0f) * (1.0f - y / w);
    return screen;
}

float Aimbot::get_fov(const Vec3& view_angles, const Vec3& target_angle) {
    float dx = target_angle.y - view_angles.y;
    float dy = target_angle.x - view_angles.x;
    
    // Normalize
    while (dx > 180.0f) dx -= 360.0f;
    while (dx < -180.0f) dx += 360.0f;
    
    return sqrtf(dx * dx + dy * dy);
}

void Aimbot::smooth_aim(Vec3& current, const Vec3& target) {
    float factor = 1.0f / config_.smooth;
    current.x += (target.x - current.x) * factor;
    current.y += (target.y - current.y) * factor;
}

} // namespace l4g::cheat