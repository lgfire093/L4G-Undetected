#include "item_esp.hpp"
#include <cmath>

namespace l4g::cheat {

ItemESP::ItemESP() = default;
ItemESP::~ItemESP() = default;

void ItemESP::set_config(const ItemESPConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

ItemESPConfig ItemESP::get_config() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void ItemESP::update_items(const std::vector<ItemInfo>& items, const float* view_matrix, int width, int height) {
    std::lock_guard<std::mutex> lock(mutex_);
    items_ = items;
    if (view_matrix) memcpy(view_matrix_, view_matrix, 16 * sizeof(float));
    screen_width_ = width;
    screen_height_ = height;
}

void ItemESP::render() {
    if (!config_.enabled) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& item : items_) {
        if (item.distance > config_.max_distance) continue;
        render_item(item);
    }
}

void ItemESP::render_item(const ItemInfo& item) {
    if (item.screen_pos.x == 0 && item.screen_pos.y == 0) return;
    
    Color color = get_item_color(item);
    std::string name = get_item_display_name(item);
    
    draw_text(item.screen_pos, name, color, true);
    
    if (config_.show_distance) {
        char buf[32];
        sprintf(buf, "%.0fm", item.distance);
        Vec2 dist_pos = {item.screen_pos.x, item.screen_pos.y + 15.0f};
        draw_text(dist_pos, buf, color, true);
    }
}

Color ItemESP::get_item_color(const ItemInfo& item) const {
    // Color by category
    std::string lower_name = item.name;
    for (auto& c : lower_name) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    
    if (lower_name.find("rifle") != std::string::npos || lower_name.find("gun") != std::string::npos ||
        lower_name.find("pistol") != std::string::npos || lower_name.find("smg") != std::string::npos ||
        lower_name.find("sniper") != std::string::npos || lower_name.find("shotgun") != std::string::npos) {
        return config_.weapon_color;
    }
    if (lower_name.find("ammo") != std::string::npos) return config_.ammo_color;
    if (lower_name.find("armor") != std::string::npos || lower_name.find("vest") != std::string::npos ||
        lower_name.find("helmet") != std::string::npos) return config_.armor_color;
    if (lower_name.find("med") != std::string::npos || lower_name.find("heal") != std::string::npos ||
        lower_name.find("bandage") != std::string::npos || lower_name.find("kit") != std::string::npos) return config_.med_color;
    if (lower_name.find("grenade") != std::string::npos || lower_name.find("molotov") != std::string::npos ||
        lower_name.find("smoke") != std::string::npos) return config_.throwable_color;
    if (lower_name.find("crate") != std::string::npos || lower_name.find("box") != std::string::npos ||
        lower_name.find("supply") != std::string::npos) return config_.crate_color;
    
    // Color by rarity
    switch (item.rarity) {
        case 1: return {0, 255, 0, 255};    // Uncommon - green
        case 2: return {0, 150, 255, 255};  // Rare - blue
        case 3: return {180, 0, 255, 255};  // Epic - purple
        case 4: return {255, 165, 0, 255};  // Legendary - orange
        default: return {255, 255, 255, 255}; // Common - white
    }
}

std::string ItemESP::get_item_display_name(const ItemInfo& item) const {
    std::string name = item.name;
    if (config_.show_rarity && item.rarity > 0) {
        const char* rarity_names[] = {"", "[U]", "[R]", "[E]", "[L]"};
        if (item.rarity <= 4) {
            name = std::string(rarity_names[item.rarity]) + " " + name;
        }
    }
    if (item.quantity > 1) {
        name += " x" + std::to_string(item.quantity);
    }
    return name;
}

} // namespace l4g::cheat