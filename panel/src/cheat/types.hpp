#pragma once
#include <cstdint>

// Shared cheat geometry/color types. esp.hpp and item_esp.hpp used to declare
// their own copies, which breaks as soon as a TU includes both.
namespace l4g::cheat {

struct Vec3 { float x, y, z; };
struct Vec2 { float x, y; };
struct Color { uint8_t r, g, b, a; };

} // namespace l4g::cheat
