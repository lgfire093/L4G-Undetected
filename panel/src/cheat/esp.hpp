#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <mutex>

namespace l4g::cheat {

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
struct Color { uint8_t r, g, b, a; };

struct EntityInfo {
    uint64_t address = 0;
    Vec3 head, neck, chest, pelvis;
    Vec3 root;
    Vec2 screen_head, screen_root;
    float distance = 0.0f;
    int health = 100;
    int max_health = 100;
    bool is_visible = false;
    bool is_teammate = false;
    bool is_knocked = false;
    std::string name;
    int team_id = 0;
};

struct ESPConfig {
    bool enabled = false;
    bool box = true;
    bool skeleton = true;
    bool health_bar = true;
    bool name = true;
    bool distance = true;
    bool weapon = false;
    bool snaplines = false;
    bool head_dot = false;
    bool filled_box = false;
    float max_distance = 300.0f;
    Color enemy_color = {255, 0, 0, 255};
    Color team_color = {0, 255, 0, 255};
    Color visible_color = {255, 255, 0, 255};
    int box_style = 0; // 0=corner, 1=full, 2=3d
    float line_width = 1.5f;
};

class ESP {
public:
    ESP();
    ~ESP();
    
    void set_config(const ESPConfig& config);
    ESPConfig get_config() const;
    
    void update_entities(const std::vector<EntityInfo>& entities, const float* view_matrix, int width, int height);
    void render();
    
    // Drawing primitives (to be implemented with overlay)
    virtual void draw_box(const Vec2& top_left, const Vec2& bottom_right, const Color& color, float thickness) = 0;
    virtual void draw_line(const Vec2& from, const Vec2& to, const Color& color, float thickness) = 0;
    virtual void draw_text(const Vec2& pos, const std::string& text, const Color& color, bool centered = true) = 0;
    virtual void draw_rect_filled(const Vec2& top_left, const Vec2& bottom_right, const Color& color) = 0;
    virtual void draw_circle(const Vec2& center, float radius, const Color& color, float thickness) = 0;
    
protected:
    ESPConfig config_;
    std::vector<EntityInfo> entities_;
    float view_matrix_[16] = {0};
    int screen_width_ = 1920;
    int screen_height_ = 1080;
    std::mutex mutex_;
    
    void render_entity(const EntityInfo& ent);
    Color get_entity_color(const EntityInfo& ent) const;
    void render_box(const EntityInfo& ent);
    void render_skeleton(const EntityInfo& ent);
    void render_health_bar(const EntityInfo& ent);
    void render_name(const EntityInfo& ent);
    void render_distance(const EntityInfo& ent);
    void render_snapline(const EntityInfo& ent);
    void render_head_dot(const EntityInfo& ent);
};

} // namespace l4g::cheat