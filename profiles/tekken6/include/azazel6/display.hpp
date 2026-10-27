#pragma once
#include "psprecomp/runtime.hpp"
#include <cstdint>
#include <optional>
#include <vector>
#include "azazel6/interrupts.hpp"

namespace azazel6 {
// Half-ticks of the 9 MHz LCD clock preserve the documented 731.5 us VBLANK interval.
// No host clock or dispatch-count conversion.
class DisplayClock {
public:
    static constexpr std::uint64_t ticks_per_second = 18000000;
    static constexpr std::uint64_t ticks_per_hcount = 2 * 525;
    static constexpr std::uint64_t hcounts_per_frame = 286;
    static constexpr std::uint64_t ticks_per_frame = ticks_per_hcount * hcounts_per_frame;
    static constexpr std::uint64_t vblank_ticks = 13167;
    static constexpr std::uint64_t max_events = 1024;
    [[nodiscard]] std::uint64_t ticks() const noexcept { return ticks_; }
    [[nodiscard]] std::uint64_t microseconds() const noexcept { return ticks_ / (ticks_per_second / 1000000); }
    [[nodiscard]] std::uint64_t frames() const noexcept { return ticks_ / ticks_per_frame; }
    [[nodiscard]] std::uint32_t current_hcount() const noexcept {
        return static_cast<std::uint32_t>(ticks_ % ticks_per_frame / ticks_per_hcount);
    }
    [[nodiscard]] bool in_vblank() const noexcept { return frames() && ticks_ % ticks_per_frame < vblank_ticks; }
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
class DisplayState {
public:
    DisplayClock clock;
    DisplayBuffers buffers;
    std::uint64_t buffer_requests = 0;
    std::uint64_t system_time_reads = 0;
    std::uint64_t hcount_reads = 0;
    bool mode_set = false; // Only the observed LCD 480x272 mode is supported.
    // An explicit time producer must return control for guest callbacks at each edge.
    void advance(psprecomp::Runtime&, std::uint64_t ticks);
    // Research-only idle jump. Never converts runnable execution into elapsed time.
    void advance_idle(psprecomp::Runtime&);
    [[nodiscard]] std::uint64_t idle_frames() const noexcept { return idle_frames_; }
private:
    friend std::shared_ptr<DisplayState> register_display_hle(psprecomp::Runtime&,
        std::shared_ptr<ThreadRegistry>, std::shared_ptr<InterruptDelivery>, std::uint64_t);
    psprecomp::Runtime* runtime_ = nullptr;
    std::shared_ptr<ThreadRegistry> threads_;
    std::shared_ptr<InterruptDelivery> interrupts_;
    std::uint64_t idle_limit_ = 0, idle_frames_ = 0;
};
std::shared_ptr<DisplayState> register_display_hle(psprecomp::Runtime&,
    std::shared_ptr<ThreadRegistry>, std::shared_ptr<InterruptDelivery>, std::uint64_t idle_frame_budget = 0);
}
