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
    if (&rt != runtime_ || !delta || delta > std::numeric_limits<std::uint64_t>::max() - clock.ticks()) {
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
    threads_->wake_timeouts(rt, rt.cpu(), clock.ticks());
    threads_->wake_display(rt, rt.cpu(), clock.frames());
    buffers.vblank();
}
}
