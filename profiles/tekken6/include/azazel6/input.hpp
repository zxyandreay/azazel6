#pragma once
#include <array>
#include <cmath>
#include <cstdint>
#include <optional>

namespace azazel6 {
// PSP button bits; sampled host state only, not a sceCtrl implementation.
enum Button : std::uint32_t {
    select = 1, start = 8, up = 0x10, right = 0x20, down = 0x40,
    left = 0x80, ltrigger = 0x100, rtrigger = 0x200,
    triangle = 0x1000, circle = 0x2000, cross = 0x4000, square = 0x8000
};
struct PadSample {
    std::uint32_t buttons = 0;
    std::uint8_t x = 128, y = 128;
};
constexpr std::uint8_t analog_axis(std::int16_t value) {
    return static_cast<std::uint8_t>((static_cast<std::int32_t>(value) + 32768) >> 8);
}
struct Touch {
    std::uint64_t device, finger;
    float x, y; // Stock 480x272 coordinates, independent of pixel density.
};
struct InputState {
    PadSample pad;
    std::array<std::optional<Touch>, 16> touches{};

    void clear() { pad = {}; touches = {}; }
    void release(std::uint64_t device, std::uint64_t finger) {
        for (auto& touch : touches)
            if (touch && touch->device == device && touch->finger == finger) touch.reset();
    }
    // Moves never create a contact. Invalid/outside moves cancel the owned contact.
    bool touch(std::uint64_t device, std::uint64_t finger, float x, float y,
               float safe_x, float safe_y, float safe_w, float safe_h, bool begin) {
        auto slot = touches.end();
        for (auto it = touches.begin(); it != touches.end(); ++it)
            if (*it && (*it)->device == device && (*it)->finger == finger) slot = it;
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(safe_x) ||
            !std::isfinite(safe_y) || !std::isfinite(safe_w) || !std::isfinite(safe_h) ||
            safe_w <= 0 || safe_h <= 0 || safe_w > 65536 || safe_h > 65536 ||
            std::abs(safe_x) > 65536 || std::abs(safe_y) > 65536) {
            release(device, finger); return false;
        }
        const float scale = std::fmin(safe_w / 480, safe_h / 272);
        const float px = (x - safe_x - (safe_w - 480 * scale) / 2) / scale;
        const float py = (y - safe_y - (safe_h - 272 * scale) / 2) / scale;
        if (!std::isfinite(px) || !std::isfinite(py) || px < 0 || px >= 480 || py < 0 || py >= 272) {
            release(device, finger); return false;
        }
        if (slot == touches.end() && begin)
            for (auto it = touches.begin(); it != touches.end(); ++it)
                if (!*it) { slot = it; break; }
        if (slot == touches.end()) return false;
        *slot = Touch{device, finger, px, py};
        return true;
    }
};
} // namespace azazel6
