#pragma once
#include "psprecomp/runtime.hpp"
#include <cstdint>
#include <optional>
#include <vector>

namespace azazel6 {
// Explicit guest time in 9 MHz LCD ticks. No host clock or dispatch-count conversion.
class DisplayClock {
public:
    static constexpr std::uint64_t ticks_per_second = 9000000;
    static constexpr std::uint64_t ticks_per_frame = 525 * 286;
    static constexpr std::uint64_t max_events = 1024;
    [[nodiscard]] std::uint64_t ticks() const noexcept { return ticks_; }
    [[nodiscard]] std::uint64_t frames() const noexcept { return ticks_ / ticks_per_frame; }
    // Validate the entire advance before mutation. Returned edges are strictly ordered.
    std::vector<std::uint64_t> advance(std::uint64_t ticks);
private:
    std::uint64_t ticks_ = 0;
};
struct Framebuffer {
    std::uint32_t address = 0, stride = 0, format = 0;
    bool operator==(const Framebuffer&) const = default;
};
class DisplayBuffers {
public:
    [[nodiscard]] const Framebuffer& current() const noexcept { return current_; }
    [[nodiscard]] const Framebuffer& latched() const noexcept { return latched_; }
    [[nodiscard]] bool pending() const noexcept { return pending_; }
    // PSP error for malformed descriptors; unsupported alias/range throws a research boundary.
    std::uint32_t set(const psprecomp::GuestMemory&, Framebuffer, std::uint32_t sync);
    void vblank() noexcept;
private:
    Framebuffer current_{}, latched_{};
    bool pending_ = false;
};
}
