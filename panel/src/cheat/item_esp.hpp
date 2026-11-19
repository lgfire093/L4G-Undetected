#pragma once
#include <windows.h>
#include <vector>
#include <string>
#include <mutex>

namespace l4g::cheat {

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
struct Color { uint8_t r, g, b, a; };

struct ItemInfo {
    uint64_t address = 0;
    Vec3 position;
    Vec2 screen_pos;
    float distance = 0.0f;
    int item_id = 0;
    std::string name;
    int rarity = 0; // 0=common, 1=uncommon, 2=rare, 3=epic, 4=legendary
    int ammo = 0;
    int quantity = 1;
};

struct ItemESPConfig {
    bool enabled = false;
    bool weapons = true;
    bool ammo = true;
    bool armor = true;
    bool meds = true;
    bool throwables = true;
    bool loot_crates = true;
    bool show_distance = true;
    bool show_rarity = true;
    float max_distance = 100.0f;
    Color weapon_color = {255, 255, 0, 255};
    Color ammo_color = {0, 255, 255, 255};
    Color armor_color = {0, 200, 255, 255};
    Color med_color = {0, 255, 0, 255};
    Color throwable_color = {255, 100, 0, 255};
    Color crate_color = {255, 0, 255, 255};
};

class ItemESP {
public:
    ItemESP();
    ~ItemESP();
    
    void set_config(const ItemESPConfig& config);
    ItemESPConfig get_config() const;
    
    void update_items(const std::vector<ItemInfo>& items, const float* view_matrix, int width, int height);
    void render();
    
    // Drawing primitives
    virtual void draw_text(const Vec2& pos, const std::string& text, const Color& color, bool centered = true) = 0;
    virtual void draw_box(const Vec2& top_left, const Vec2& bottom_right, const Color& color, float thickness) = 0;
    
protected:
    ItemESPConfig config_;
    std::vector<ItemInfo> items_;
    float view_matrix_[16] = {0};
    int screen_width_ = 1920;
    int screen_height_ = 1080;
    std::mutex mutex_;
    
    void render_item(const ItemInfo& item);
    Color get_item_color(const ItemInfo& item) const;
    std::string get_item_display_name(const ItemInfo& item) const;
};

} // namespace l4g::cheat