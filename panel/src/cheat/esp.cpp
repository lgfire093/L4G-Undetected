#include "esp.hpp"
#include <algorithm>
#include <cmath>

namespace l4g::cheat {

ESP::ESP() = default;
ESP::~ESP() = default;

void ESP::set_config(const ESPConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

ESPConfig ESP::get_config() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void ESP::update_entities(const std::vector<EntityInfo>& entities, const float* view_matrix, int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    entities_ = entities;
    if (view_matrix) memcpy(view_matrix_, view_matrix, 16 * sizeof(float));
    screen_width_ = width;
    screen_height_ = height;
}

void ESP::render() {
    if (!config_.enabled) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& ent : entities_) {
        if (ent.distance > config_.max_distance) continue;
        render_entity(ent);
    }
}

void ESP::render_entity(const EntityInfo& ent) {
    Color color = get_entity_color(ent);
    
    if (config_.box) render_box(ent);
    if (config_.skeleton) render_skeleton(ent);
    if (config_.health_bar) render_health_bar(ent);
    if (config_.name) render_name(ent);
    if (config_.distance) render_distance(ent);
    if (config_.snaplines) render_snapline(ent);
    if (config_.head_dot) render_head_dot(ent);
}

Color ESP::get_entity_color(const EntityInfo& ent) const {
    if (ent.is_teammate) return config_.team_color;
    if (ent.is_visible) return config_.visible_color;
    return config_.enemy_color;
}

void ESP::render_box(const EntityInfo& ent) {
    if (ent.screen_head.x == 0 && ent.screen_head.y == 0) return;
    if (ent.screen_root.x == 0 && ent.screen_root.y == 0) return;
    
    float height = fabsf(ent.screen_head.y - ent.screen_root.y);
    float width = height * 0.5f;
    
    Vec2 top_left = {ent.screen_head.x - width / 2, ent.screen_head.y};
    Vec2 bottom_right = {ent.screen_head.x + width / 2, ent.screen_root.y};
    Color color = get_entity_color(ent);
    
    if (config_.filled_box) {
        draw_rect_filled(top_left, bottom_right, {color.r, color.g, color.b, 80});
    }
    
    switch (config_.box_style) {
        case 0: // Corner box
            draw_line({top_left.x, top_left.y}, {top_left.x + width * 0.3f, top_left.y}, color, config_.line_width);
            draw_line({top_left.x, top_left.y}, {top_left.x, top_left.y + height * 0.3f}, color, config_.line_width);
            draw_line({bottom_right.x, top_left.y}, {bottom_right.x - width * 0.3f, top_left.y}, color, config_.line_width);
            draw_line({bottom_right.x, top_left.y}, {bottom_right.x, top_left.y + height * 0.3f}, color, config_.line_width);
            draw_line({top_left.x, bottom_right.y}, {top_left.x + width * 0.3f, bottom_right.y}, color, config_.line_width);
            draw_line({top_left.x, bottom_right.y}, {top_left.x, bottom_right.y - height * 0.3f}, color, config_.line_width);
            draw_line({bottom_right.x, bottom_right.y}, {bottom_right.x - width * 0.3f, bottom_right.y}, color, config_.line_width);
            draw_line({bottom_right.x, bottom_right.y}, {bottom_right.x, bottom_right.y - height * 0.3f}, color, config_.line_width);
            break;
        case 1: // Full box
            draw_line(top_left, {bottom_right.x, top_left.y}, color, config_.line_width);
            draw_line({bottom_right.x, top_left.y}, bottom_right, color, config_.line_width);
            draw_line(bottom_right, {top_left.x, bottom_right.y}, color, config_.line_width);
            draw_line({top_left.x, bottom_right.y}, top_left, color, config_.line_width);
            break;
    }
}

void ESP::render_skeleton(const EntityInfo& ent) {
    Color color = get_entity_color(ent);
    // Head -> Neck -> Chest -> Pelvis
    draw_line(ent.screen_head, {ent.neck.x, ent.neck.y}, color, config_.line_width);
    draw_line({ent.neck.x, ent.neck.y}, {ent.chest.x, ent.chest.y}, color, config_.line_width);
    draw_line({ent.chest.x, ent.chest.y}, {ent.pelvis.x, ent.pelvis.y}, color, config_.line_width);
    // Arms
    // Legs
}

void ESP::render_health_bar(const EntityInfo& ent) {
    if (ent.screen_head.x == 0 && ent.screen_head.y == 0) return;
    
    float height = fabsf(ent.screen_head.y - ent.screen_root.y);
    float bar_width = 4.0f;
    float bar_x = ent.screen_head.x - height * 0.5f - 6.0f;
    float bar_y = ent.screen_head.y;
    float health_pct = (float)ent.health / (float)ent.max_health;
    float health_height = height * health_pct;
    
    Color bg = {0, 0, 0, 200};
    Color fg = {0, 255, 0, 255};
    if (health_pct < 0.5f) fg = {255, 255, 0, 255};
    if (health_pct < 0.25f) fg = {255, 0, 0, 255};
    
    draw_rect_filled({bar_x, bar_y}, {bar_x + bar_width, bar_y + height}, bg);
    draw_rect_filled({bar_x, bar_y + height - health_height}, {bar_x + bar_width, bar_y + height}, fg);
}

void ESP::render_name(const EntityInfo& ent) {
    if (ent.screen_head.x == 0 && ent.screen_head.y == 0) return;
    if (ent.name.empty()) return;
    
    Vec2 pos = {ent.screen_head.x, ent.screen_head.y - 15.0f};
    Color color = get_entity_color(ent);
    draw_text(pos, ent.name, color, true);
}

void ESP::render_distance(const EntityInfo& ent) {
    if (ent.screen_root.x == 0 && ent.screen_root.y == 0) return;
    
    char buf[32];
    sprintf(buf, "%.0fm", ent.distance);
    Vec2 pos = {ent.screen_root.x, ent.screen_root.y + 15.0f};
    Color color = get_entity_color(ent);
    draw_text(pos, buf, color, true);
}

void ESP::render_snapline(const EntityInfo& ent) {
    if (ent.screen_root.x == 0 && ent.screen_root.y == 0) return;
    
    Vec2 from = {(float)screen_width_ / 2, (float)screen_height_};
    Vec2 to = ent.screen_root;
    Color color = get_entity_color(ent);
    draw_line(from, to, color, 1.0f);
}

void ESP::render_head_dot(const EntityInfo& ent) {
    if (ent.screen_head.x == 0 && ent.screen_head.y == 0) return;
    
    Color color = get_entity_color(ent);
    draw_circle(ent.screen_head, 4.0f, color, 2.0f);
}

} // namespace l4g::cheat