#include "azazel6/display.hpp"
#include "psprecomp/common.hpp"
#include <limits>
#include <algorithm>

namespace azazel6 {
std::vector<std::uint64_t> DisplayClock::advance(std::uint64_t delta) {
    if (delta > std::numeric_limits<std::uint64_t>::max() - ticks_)
        throw psprecomp::Error("Display clock overflow");
    const auto next = ticks_ + delta;
    const auto count = next / ticks_per_frame - frames();
    if (count > max_events) throw psprecomp::Error("Display event budget exhausted");
    std::vector<std::uint64_t> edges;
    edges.reserve(static_cast<std::size_t>(count));
    for (auto i = frames(); i < next / ticks_per_frame; ++i)
        edges.push_back((i + 1) * ticks_per_frame);
    ticks_ = next;
    return edges;
}
namespace {
void check_range(const psprecomp::GuestMemory& mem, Framebuffer f) {
    if (!f.address) return; // Display off owns no guest memory.
    const auto bytes = static_cast<std::uint64_t>(f.stride) * 272 * (f.format == 3 ? 4 : 2);
    const auto end = static_cast<std::uint64_t>(f.address) + bytes;
    const bool ram = f.address >= psprecomp::GuestMemory::kPhysicalBase &&
        end <= psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(mem.size());
    const bool vram = f.address >= psprecomp::GuestMemory::kVramPhysicalBase &&
        end <= psprecomp::GuestMemory::kVramPhysicalBase + static_cast<std::uint64_t>(mem.vram_size());
    if (!ram && !vram) throw psprecomp::Error("Framebuffer outside bounded canonical RAM/VRAM");
    if (f.stride < 480) throw psprecomp::Error("Framebuffer stride below stock LCD width is unsupported");
}
}
std::uint32_t DisplayBuffers::set(const psprecomp::GuestMemory& mem, Framebuffer f, std::uint32_t sync) {
    if (sync > 1) return 0x80000107u;
    if (f.address & 15u) return 0x80000103u;
    if ((f.stride & 63u) || (!f.stride && f.address)) return 0x80000104u;
    if (f.format > 3) return 0x80000108u;
    if (!sync && (f.stride != latched_.stride || f.format != latched_.format)) return 0x80000107u;
    check_range(mem, f);
    // Next-frame updates change current format/stride before its base latches.
    // Validate that intermediate descriptor as well, before changing either state.
    auto intermediate = current_;
    intermediate.stride = f.stride; intermediate.format = f.format;
    if (sync) check_range(mem, intermediate);
    latched_ = f;
    if (sync) { current_ = intermediate; pending_ = true; }
    else { current_ = f; pending_ = false; }
    return 0;
}
void DisplayBuffers::vblank() noexcept {
    if (pending_) { current_ = latched_; pending_ = false; }
}
std::vector<std::uint8_t> DisplayBuffers::readback_rgba(const psprecomp::GuestMemory& memory) const {
    const auto f = current_;
    if (!f.address) return {};
    check_range(memory, f);
    std::vector<std::uint8_t> result(480 * 272 * 4);
    const auto expand5 = [](std::uint32_t v) { return (v << 3) | (v >> 2); };
    const auto expand6 = [](std::uint32_t v) { return (v << 2) | (v >> 4); };
    for (std::uint32_t y = 0; y < 272; ++y) for (std::uint32_t x = 0; x < 480; ++x) {
        const auto address = f.address + (y * f.stride + x) * (f.format == 3 ? 4 : 2);
        const std::uint32_t pixel = f.format == 3 ? memory.load32(address) : memory.load16(address);
        std::uint32_t r, g, b, a;
        switch (f.format) {
        case 0: // PSP 5650: red occupies the low five bits, no stored alpha.
            r = expand5(pixel & 31); g = expand6((pixel >> 5) & 63);
            b = expand5((pixel >> 11) & 31); a = 255; break;
        case 1:
            r = expand5(pixel & 31); g = expand5((pixel >> 5) & 31);
            b = expand5((pixel >> 10) & 31); a = (pixel >> 15) * 255; break;
        case 2:
            r = (pixel & 15) * 17; g = ((pixel >> 4) & 15) * 17;
            b = ((pixel >> 8) & 15) * 17; a = ((pixel >> 12) & 15) * 17; break;
        default: // Validated 8888 descriptor; preserve stored alpha for diagnostics.
            r = pixel & 255; g = (pixel >> 8) & 255;
            b = (pixel >> 16) & 255; a = pixel >> 24; break;
        }
        const auto output = (y * 480 + x) * 4;
        result[output] = static_cast<std::uint8_t>(r);
        result[output + 1] = static_cast<std::uint8_t>(g);
        result[output + 2] = static_cast<std::uint8_t>(b);
        result[output + 3] = static_cast<std::uint8_t>(a);
    }
    return result;
}
std::shared_ptr<DisplayState> register_display_hle(psprecomp::Runtime& rt,
    std::shared_ptr<ThreadRegistry> threads, std::shared_ptr<InterruptDelivery> interrupts,
    std::uint64_t idle_frame_budget) {
    if (!threads || !interrupts) throw psprecomp::Error("Display requires owned threads and interrupts");
    if (idle_frame_budget > DisplayClock::max_events) throw psprecomp::Error("Idle frame budget exceeds limit");
    auto state = std::make_shared<DisplayState>();
    state->runtime_ = &rt; state->threads_ = std::move(threads); state->interrupts_ = std::move(interrupts);
    state->idle_limit_ = idle_frame_budget;
    rt.register_hle("ThreadManForUser", 0x369ED59Du, [state](auto&, auto& c) {
        // Global time is readable during GE/VBLANK interrupts. It comes only
        // from the explicit guest event producer, never host time or read count.
        c.set_gpr(2, static_cast<std::uint32_t>(state->clock.microseconds()));
        ++state->system_time_reads;
    });
    rt.register_hle("ThreadManForUser", 0x82BC5777u, [state](auto&, auto& c) {
        const auto microseconds = state->clock.microseconds();
        c.set_gpr(2, static_cast<std::uint32_t>(microseconds));
        c.set_gpr(3, static_cast<std::uint32_t>(microseconds >> 32));
        ++state->system_time_reads;
    });
    rt.register_hle("sceDisplay", 0x773DD3A3u, [state](auto&, auto& c) {
        c.set_gpr(2, state->clock.current_hcount());
        ++state->hcount_reads;
    });
    if (idle_frame_budget) {
        rt.register_hle("Azazel6Host", 3, [state](auto& runtime, auto&) {
            try { state->advance_idle(runtime); }
            catch (const psprecomp::Error& e) { runtime.stop(e.what()); }
        });
        rt.register_function(kDisplayIdleGate, [](auto& runtime, auto& c) {
            runtime.invoke_import("Azazel6Host", 3, c);
        }, "host_bounded_display_idle");
    }
    rt.register_hle("sceDisplay", 0x289D82FEu, [state](auto& runtime, auto& c) {
        try {
            const auto result = state->buffers.set(runtime.memory(), {c.gpr[4], c.gpr[5], c.gpr[6]}, c.gpr[7]);
            if (!result) ++state->buffer_requests;
            c.set_gpr(2, result);
        } catch (const psprecomp::Error& e) { runtime.stop(e.what()); }
    });
    rt.register_hle("sceDisplay", 0x36CDFADEu, [state](auto& runtime, auto& c) {
        if (state->threads_->in_interrupt() || !state->threads_->interrupts_enabled()) {
            runtime.stop("Display wait requires an interruptible thread context"); return;
        }
        if (state->clock.in_vblank()) { c.set_gpr(2, 0); return; }
        state->threads_->wait_display(runtime, c, state->clock.frames() + 1);
    });
    rt.register_hle("sceDisplay", 0x46F186C3u, [state](auto& runtime, auto& c) {
        // Only inactive callbacks exist in this profile. NotifyCallback and
        // asynchronous exit events still stop rather than queueing hidden work.
        // Unlike WaitVblank, StartCB must wait for a NEW edge even in VBLANK.
        state->threads_->wait_display(runtime, c, state->clock.frames() + 1);
    });
    rt.register_hle("sceDisplay", 0x0E20F177u, [state](auto& runtime, auto& c) {
        if (c.gpr[4]) { c.set_gpr(2, 0x80000107u); return; }
        if (c.gpr[5] != 480 || c.gpr[6] != 272) { c.set_gpr(2, 0x80000104u); return; }
        // SetMode waits for the next VBLANK even if called during the current one.
        if (state->threads_->wait_display(runtime, c, state->clock.frames() + 1))
            state->mode_set = true;
    });
    return state;
}
bool DisplayState::wait_timeout(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c,
                                std::uint32_t microseconds, std::uint32_t result) {
    const auto delta = static_cast<std::uint64_t>(microseconds) * (DisplayClock::ticks_per_second / 1000000);
    if (&rt != runtime_ || &c != &rt.cpu() || !delta || delta > std::numeric_limits<std::uint64_t>::max() - clock.ticks()) {
        rt.stop("Invalid timed-wait clock ownership, duration or overflow"); return false;
    }
    return threads_->wait_timeout(rt, c, clock.ticks() + delta, result);
}
void DisplayState::advance_idle(psprecomp::Runtime& rt) {
    if (!idle_limit_ || idle_events_ >= idle_limit_) throw psprecomp::Error("Idle display frame budget exhausted");
    if (!threads_->can_idle_display() || rt.cpu().pc != kDisplayIdleGate)
        throw psprecomp::Error("Idle display advance requires all threads blocked and a display or timed waiter");
    auto delta = DisplayClock::ticks_per_frame - clock.ticks() % DisplayClock::ticks_per_frame;
    if (const auto deadline = threads_->next_timeout()) {
        if (*deadline <= clock.ticks()) throw psprecomp::Error("Stale timed wait deadline");
        delta = std::min(delta, *deadline - clock.ticks());
    }
    const auto before = clock.frames();
    advance(rt, delta);
    idle_frames_ += clock.frames() - before;
    ++idle_events_;
}
void DisplayState::advance(psprecomp::Runtime& rt, std::uint64_t ticks) {
    if (&rt != runtime_) throw psprecomp::Error("Display runtime ownership mismatch");
    if (rt.stopped() || interrupts_->active() || interrupts_->pending())
        throw psprecomp::Error("Display advance requires a completed interrupt boundary");
    const auto next = DisplayClock::ticks_per_frame - clock.ticks() % DisplayClock::ticks_per_frame;
    if (ticks > next) throw psprecomp::Error("Display advance must yield at the next VBLANK edge");
    if (const auto deadline = threads_->next_timeout(); deadline &&
        (*deadline < clock.ticks() || ticks > *deadline - clock.ticks()))
        throw psprecomp::Error("Clock advance must yield at the next timeout");
    const auto edges = clock.advance(ticks);
    if (edges.empty()) { threads_->wake_timeouts(rt, rt.cpu(), clock.ticks()); return; }
    interrupts_->raise_vblank();
    interrupts_->dispatch(rt, rt.cpu());
    if (rt.stopped()) return;
    buffers.vblank();
    // Publish all state for this event before selecting a runnable thread.
    threads_->wake_clock(rt, rt.cpu(), clock.ticks(), clock.frames());
}
}
