#include "azazel6/display.hpp"
#include "azazel6/bootstrap.hpp"
#include "azazel6/threads.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <memory>

namespace {
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class F> void rejects(F f, const char* why) {
    bool caught = false;
    try { f(); } catch (const std::exception&) { caught = true; }
    check(caught, why);
}
void wait_contracts() {
    using namespace azazel6;
    auto rt = std::make_unique<psprecomp::Runtime>();
    auto threads = register_thread_hle(*rt,
        {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
         0x08001000u, 0x08002000u, 0x08003000u, 32, 4096});
    auto& c = rt->cpu(); c.pc = 0x08001000u; c.gpr[31] = 0x08001004u; c.gpr[2] = 0xDEADBEEFu;
    c.vfpu[127] = 123.0f; c.gpr[16] = 42;
    check(threads->wait_display(*rt, c, 2) && !rt->stopped() && threads->current_uid() == 0 &&
          c.pc == kDisplayIdleGate, "Wait returned early instead of entering idle");
    threads->wake_display(*rt, c, 1);
    check(threads->current_uid() == 0 && c.pc == kDisplayIdleGate, "Wait woke before its edge");
    threads->wake_display(*rt, c, 2);
    check(threads->current_uid() == kBootstrapUid && c.pc == 0x08001004u && c.gpr[2] == 0 &&
          c.gpr[16] == 42 && c.vfpu[127] == 123.0f, "Wait context/completion");
    c.gpr[31] = 0x08001008u;
    check(threads->wait_display(*rt, c, 3) && threads->cancel_display(*rt, c, kBootstrapUid), "Wait cancellation");
    check(c.gpr[2] == 0x800201A9u && c.pc == 0x08001008u &&
          !threads->cancel_display(*rt, c, kBootstrapUid), "Cancelled result/duplicate cancellation");
    threads->wake_display(*rt, c, 3);
    check(c.gpr[2] == 0x800201A9u, "Stale edge overwrote cancellation");
    rt->invoke_import("Kernel_Library", 0x092968F4u, c);
    check(!threads->wait_display(*rt, c, 4) && rt->stopped(), "Wait while interrupts suspended");
}
void framebuffer_readback_contracts() {
    using namespace azazel6;
    auto rt = std::make_unique<psprecomp::Runtime>();
    struct Pixel { std::uint32_t format, packed; std::array<std::uint8_t, 4> rgba; };
    const Pixel cases[] = {
        {0, 0x001F, {255, 0, 0, 255}}, {0, 0x07E0, {0, 255, 0, 255}},
        {0, 0xF800, {0, 0, 255, 255}}, {0, 0x8410, {132, 130, 132, 255}},
        {1, 0x7C1F, {255, 0, 255, 0}}, {1, 0x83E0, {0, 255, 0, 255}},
        {2, 0xA531, {17, 51, 85, 170}}, {3, 0x78563412, {18, 52, 86, 120}}
    };
    for (const auto base : {0x04000000u, 0x08010000u}) for (const auto& test : cases) {
        DisplayBuffers buffers;
        const auto bytes = test.format == 3 ? 4u : 2u;
        for (std::uint32_t y = 0; y < 272; ++y) for (std::uint32_t x = 0; x < 512; ++x) {
            const auto value = x < 480 ? test.packed : 0xDEADBEEFu;
            if (bytes == 4) rt->memory().store32(base + (y * 512 + x) * bytes, value);
            else rt->memory().store16(base + (y * 512 + x) * bytes, static_cast<std::uint16_t>(value));
        }
        check(buffers.set(rt->memory(), {base, 512, test.format}, 1) == 0, "Readback setup");
        check(buffers.readback_rgba(rt->memory()).empty() && buffers.pending(), "Pending base scanned out early");
        buffers.vblank();
        const auto pixels = buffers.readback_rgba(rt->memory());
        check(pixels.size() == 480 * 272 * 4, "Readback shape includes padding");
        for (std::size_t i = 0; i < pixels.size(); ++i)
            check(pixels[i] == test.rgba[i % 4], "Readback format/channel/stride mismatch");
        check(buffers.set(rt->memory(), {0, 512, test.format}, 1) == 0, "Readback disable setup");
        check(buffers.readback_rgba(rt->memory()) == pixels && buffers.pending(), "Readback mutated pending latch");
        buffers.vblank();
        check(buffers.readback_rgba(rt->memory()).empty(), "Disabled display returned pixels");
        check(pixels[0] == test.rgba[0], "Snapshot changed after latch");
    }
}
void mixed_wait_contracts() {
    using namespace azazel6;
    for (const auto priority : {16u, 32u, 48u}) {
        auto rt = std::make_unique<psprecomp::Runtime>();
        auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
        auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
            0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
        auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
        auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
        auto display = register_display_hle(*rt, threads, delivery, 3);
        rt->register_function(0x08001000u, [](auto&, auto&) {}, "synthetic_worker");
        auto& c = rt->cpu();
        c.gpr[4] = 0x08004000u; c.gpr[5] = 0x08001000u; c.gpr[6] = priority;
        c.gpr[7] = 1024; c.gpr[8] = 0; c.gpr[9] = 0;
        rt->invoke_import("ThreadManForUser", kCreateThreadNid, c);
        const auto child = c.gpr[2];
        c.gpr[4] = child; c.gpr[5] = 0; c.gpr[6] = 0;
        rt->invoke_import("ThreadManForUser", kStartThreadNid, c);
        // A higher-priority child runs first; block each on its respective event.
        for (int i = 0; i < 2; ++i) {
            if (threads->current_uid() == kBootstrapUid) {
                c.gpr[31] = 0x08001004u; c.gpr[16] = 11;
                check(threads->wait_display(*rt, c, 3), "Bootstrap display wait");
            } else {
                check(threads->current_uid() == child, "Child not scheduled");
                c.gpr[31] = 0x08001008u; c.gpr[16] = 22;
                check(display->wait_timeout(*rt, c, 50050, 0x800201A8u), "Child timed wait");
            }
        }
        check(!rt->stopped() && threads->can_idle_display(), "Mixed wait did not idle");
        display->buffers.set(rt->memory(), {0x04000000u, 512, 3}, 1);
        display->advance_idle(*rt);
        display->advance_idle(*rt);
        check(threads->current_uid() == 0 && display->idle_frames() == 2, "Mixed wait woke early");
        display->advance_idle(*rt);
        const bool child_first = priority < 32;
        check(!rt->stopped() && threads->current_uid() == (child_first ? child : kBootstrapUid) &&
              c.pc == (child_first ? 0x08001008u : 0x08001004u) &&
              c.gpr[16] == (child_first ? 22u : 11u) &&
              c.gpr[2] == (child_first ? 0x800201A8u : 0u),
              "Clock event scheduled before all waiters were ready");
        check(!threads->next_timeout() && display->buffers.current().address == 0x04000000u &&
              display->idle_events() == 3, "Clock event left stale timeout or framebuffer");
    }
}
void short_timeout_budget() {
    using namespace azazel6;
    auto rt = std::make_unique<psprecomp::Runtime>();
    auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
    auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
        0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
    auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
    auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
    auto display = register_display_hle(*rt, threads, delivery, 2);
    auto& c = rt->cpu();
    for (int i = 0; i < 2; ++i) {
        c.gpr[31] = 0x08001004u;
        check(display->wait_timeout(*rt, c, 1, 0), "Short timeout setup");
        display->advance_idle(*rt);
    }
    check(display->idle_events() == 2 && display->idle_frames() == 0 && display->clock.ticks() == 36,
          "Subframe timeouts not charged to event budget");
    check(display->wait_timeout(*rt, c, 1, 0), "Final short timeout setup");
    rejects([&] { display->advance_idle(*rt); }, "Subframe events escaped idle budget");
    check(display->clock.ticks() == 36 && threads->current_uid() == 0 &&
          threads->next_timeout() == 54, "Budget rejection mutated wait state");
}
void clock_context_boundaries() {
    using namespace azazel6;
    for (bool bootstrap : {false, true}) {
        auto rt = std::make_unique<psprecomp::Runtime>();
        auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
        auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
            0x08001000u, 0x08002000u, 0x08003000u, bootstrap ? 32u : 0u, bootstrap ? 4096u : 0u}, allocator);
        auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
        auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
        auto display = register_display_hle(*rt, threads, delivery);
        // An event that wakes nobody must not invoke the scheduler or stop an empty host.
        display->advance(*rt, 0);
        display->advance(*rt, 1);
        check(!rt->stopped() && display->clock.ticks() == 1, "Empty clock advance scheduled guest work");
        if (bootstrap) {
            auto foreign = rt->cpu();
            check(!display->wait_timeout(*rt, foreign, 1, 0) && rt->stopped() &&
                  threads->current_uid() == kBootstrapUid && !threads->next_timeout() &&
                  display->clock.ticks() == 1, "Foreign CPU context entered timed wait");
        }
    }
}
void startup_delay_contracts() {
    using namespace azazel6;
    for (unsigned scenario = 0; scenario < 5; ++scenario) {
        auto rt = std::make_unique<psprecomp::Runtime>();
        auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
        auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
            0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
        auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
        auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
        auto display = register_display_hle(*rt, threads, delivery, 8);
        auto& c = rt->cpu();
        c.pc = 0x08001000u; c.gpr[31] = 0x08001004u; c.gpr[16] = 99; c.vfpu[127] = 42;
        c.gpr[4] = scenario == 1 ? 0 : scenario == 2 ? 1 : 96000;
        if (scenario == 3) threads->enter_interrupt();
        if (scenario == 4) rt->invoke_import("Kernel_Library", 0x092968F4u, c);
        rt->invoke_import("ThreadManForUser", 0xCEADEB47u, c);
        if (scenario) {
            check(rt->stopped() && !threads->next_timeout() && display->clock.ticks() == 0 &&
                  threads->current_uid() == kBootstrapUid, "Rejected delay mutated scheduler");
            continue;
        }
        check(!rt->stopped() && threads->current_uid() == 0 && display->clock.ticks() == 0,
              "Delay returned immediately");
        rt->register_function(0x08001004u, [](auto& runtime, auto&) {
            runtime.stop("synthetic delay complete");
        }, "synthetic_delay_return");
        rt->run(c.pc, 16);
        check(rt->stop_reason() == "synthetic delay complete" && display->clock.microseconds() == 96010 &&
              c.gpr[2] == 0 && c.gpr[16] == 99 && c.vfpu[127] == 42 && !threads->next_timeout() &&
              display->idle_frames() == 5 && display->idle_events() == 6,
              "Delay deadline, context or LCD ordering");
    }
}
void event_contracts(bool idle, bool timed = false) {
    using namespace azazel6;
    auto rt = std::make_unique<psprecomp::Runtime>();
    auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
    auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
        0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
    auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
    auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
    auto display = register_display_hle(*rt, threads, delivery, idle ? (timed ? 3 : 1) : 0);
    rejects([&] { display->advance_idle(*rt); }, "Idle advance while guest runnable");
    rt->register_function(0x08001000u, [](auto& runtime, auto& c) {
        runtime.memory().store32(c.gpr[5], 1);
        c.gpr[16] = 0xBADu; c.fcr31 = 0; c.vfpu[127] = 0;
        c.pc = c.gpr[31];
    }, "synthetic_vblank");
    rt->register_function(0x08001004u, [](auto& runtime, auto&) { runtime.stop("synthetic frame complete"); },
                          "synthetic_continuation");
    auto& c = rt->cpu();
    c.gpr[4] = 30; c.gpr[5] = 0; c.gpr[6] = 0x08001000u; c.gpr[7] = 0x08004000u;
    rt->invoke_import("InterruptManager", 0xCA04A2B9u, c);
    rt->invoke_import("InterruptManager", 0xFB8E22ECu, c);
    c.gpr[4] = 0x04000000u; c.gpr[5] = 512; c.gpr[6] = 3; c.gpr[7] = 1;
    rt->invoke_import("sceDisplay", 0x289D82FEu, c);
    check(c.gpr[2] == 0 && display->buffer_requests == 1 && display->buffers.pending(), "Framebuffer HLE");
    c.pc = 0x08001008u; c.gpr[31] = 0x08001004u; c.gpr[16] = 99; c.fcr31 = 23; c.vfpu[127] = 123;
    if (timed) display->wait_timeout(*rt, c, 50050, 0);
    else rt->invoke_import("sceDisplay", 0x36CDFADEu, c);
    rejects([&] { display->advance(*rt, DisplayClock::ticks_per_frame + 1); }, "Multiple unscheduled edges accepted");
    check(display->clock.ticks() == 0, "Rejected advance changed time");
    if (!idle) {
        display->advance(*rt, DisplayClock::ticks_per_frame);
        check(delivery->active() && threads->current_uid() == 0 && c.pc == 0x08001000u &&
              display->buffers.current().address == 0x04000000u, "Interrupt-before-wakeup and latch order");
        rejects([&] { display->advance(*rt, 1); }, "Clock advanced through an unfinished handler");
    }
    rt->run(c.pc, 32);
    check(rt->stop_reason() == "synthetic frame complete" && rt->memory().load32(0x08004000u) == 1 &&
          threads->current_uid() == kBootstrapUid && c.gpr[2] == 0 && c.gpr[16] == 99 &&
          c.fcr31 == 23 && c.vfpu[127] == 123 && !delivery->active() &&
          display->clock.frames() == (timed ? 3 : 1) && display->idle_frames() == (idle ? (timed ? 3 : 1) : 0),
          "Integrated callback return and waiting context");
}
void idle_boundaries() {
    using namespace azazel6;
    auto rt = std::make_unique<psprecomp::Runtime>();
    auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
    auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
        0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
    auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
    auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
    rejects([&] { register_display_hle(*rt, threads, delivery, 1025); }, "Unbounded idle producer");
    auto display = register_display_hle(*rt, threads, delivery, 1);
    threads->wait_display(*rt, rt->cpu(), 2);
    auto other = std::make_unique<psprecomp::Runtime>(); other->cpu().pc = kDisplayIdleGate;
    rejects([&] { display->advance_idle(*other); }, "Cross-runtime idle producer");
    check(display->clock.ticks() == 0 && display->idle_frames() == 0, "Rejected idle changed clock");
    rt->run(kDisplayIdleGate, 4);
    check(rt->stop_reason() == "Idle display frame budget exhausted" && display->idle_frames() == 1 &&
          display->clock.frames() == 1 && threads->current_uid() == 0, "Idle budget or early wake");
}
void mode_contracts() {
    using namespace azazel6;
    auto rt = std::make_unique<psprecomp::Runtime>();
    auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
    auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
        0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
    auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
    auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
    auto display = register_display_hle(*rt, threads, delivery, 1);
    auto& c = rt->cpu();
    c.gpr[4] = 1; c.gpr[5] = 0; c.gpr[6] = 0;
    rt->invoke_import("sceDisplay", 0x0E20F177u, c);
    check(c.gpr[2] == 0x80000107u && !display->mode_set, "Display mode precedence");
    c.gpr[4] = 0; c.gpr[5] = 480; c.gpr[6] = 271;
    rt->invoke_import("sceDisplay", 0x0E20F177u, c);
    check(c.gpr[2] == 0x80000104u && !display->mode_set, "Display dimensions");
    display->advance(*rt, DisplayClock::ticks_per_frame);
    check(display->clock.in_vblank(), "SetMode test must start within VBLANK");
    c.gpr[6] = 272; c.gpr[31] = 0x08001004u;
    rt->register_function(0x08001004u, [](auto& runtime, auto&) { runtime.stop("synthetic mode ready"); }, "mode_ready");
    rt->invoke_import("sceDisplay", 0x0E20F177u, c);
    check(display->mode_set && threads->current_uid() == 0 && display->clock.frames() == 1,
          "SetMode completed in the same VBLANK");
    rt->run(c.pc, 4);
    check(rt->stop_reason() == "synthetic mode ready" && display->clock.frames() == 2 &&
          c.gpr[2] == 0 && threads->current_uid() == kBootstrapUid, "SetMode next-edge wakeup");
}
void system_time_contracts() {
    using namespace azazel6;
    auto rt = std::make_unique<psprecomp::Runtime>();
    auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
    auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
        0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
    auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
    auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
    auto display = register_display_hle(*rt, threads, delivery);
    auto query_wide = [&](std::uint64_t expected) {
        auto& c = rt->cpu(); c.pc = 0x08001000u; c.gpr[31] = 0x08001004u;
        c.gpr[2] = 42; c.gpr[3] = 43; c.gpr[4] = 0xFFFFFFFFu; c.gpr[16] = 77;
        const auto before = c; const auto ticks = display->clock.ticks();
        const auto reads = display->system_time_reads;
        invoke_returning_import(*rt, c, "ThreadManForUser", 0x82BC5777u);
        check(!rt->stopped() && c.gpr[2] == static_cast<std::uint32_t>(expected) &&
              c.gpr[3] == static_cast<std::uint32_t>(expected >> 32) && c.pc == 0x08001004u &&
              display->clock.ticks() == ticks && display->system_time_reads == reads + 1,
              "Wide system time query advanced time or violated return ABI");
        for (unsigned i = 0; i < 32; ++i) if (i != 2 && i != 3)
            check(c.gpr[i] == before.gpr[i], "Wide clock query clobbered register");
    };
    auto query = [&](std::uint32_t expected) {
        auto& c = rt->cpu(); c.pc = 0x08001000u; c.gpr[31] = 0x08001004u;
        c.gpr[2] = 42; c.gpr[4] = 0xFFFFFFFFu; c.gpr[16] = 77;
        const auto before = c; const auto ticks = display->clock.ticks();
        const auto reads = display->system_time_reads;
        invoke_returning_import(*rt, c, "ThreadManForUser", 0x369ED59Du);
        check(!rt->stopped() && c.gpr[2] == expected && c.pc == 0x08001004u &&
              display->clock.ticks() == ticks && display->system_time_reads == reads + 1,
              "System time query advanced time or violated return ABI");
        for (unsigned i = 0; i < 32; ++i) if (i != 2) check(c.gpr[i] == before.gpr[i], "Clock query clobbered register");
    };
    query_wide(0); query(0); display->clock.advance(17); query(0);
    display->clock.advance(1); query(1); query(1);
    display->clock.advance(DisplayClock::ticks_per_frame - 18); query(16683);
    display->clock.advance(DisplayClock::ticks_per_frame); query(33366);
    display->clock.advance(DisplayClock::ticks_per_frame); query(50050);
    check(threads->enter_interrupt(), "Clock interrupt fixture"); query(50050);
    threads->leave_interrupt();
    rt->invoke_import("Kernel_Library", 0x092968F4u, rt->cpu()); query(50050);
    // Synthetic counter producer only: exercise low-word wrap without sleeping
    // or inventing elapsed time in the HLE query itself.
    const auto wrap = (std::uint64_t{1} << 32) * 18;
    while (display->clock.ticks() < wrap)
        display->clock.advance(std::min(wrap - display->clock.ticks(),
            DisplayClock::ticks_per_frame * DisplayClock::max_events));
    query_wide(std::uint64_t{1} << 32); query(0);
    display->clock.advance(7 * 18); query(7); query_wide((std::uint64_t{1} << 32) + 7);
}
void hcount_contracts() {
    using namespace azazel6;
    auto rt = std::make_unique<psprecomp::Runtime>();
    auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
    auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
        0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
    auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
    auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
    auto display = register_display_hle(*rt, threads, delivery);
    auto query = [&](std::uint32_t expected) {
        auto& c = rt->cpu(); c.pc = 0x08001000u; c.gpr[31] = 0x08001004u;
        c.gpr[2] = 42; c.gpr[4] = 0xFFFFFFFFu; c.gpr[16] = 77;
        const auto before = c; const auto ticks = display->clock.ticks();
        const auto reads = display->hcount_reads;
        invoke_returning_import(*rt, c, "sceDisplay", 0x773DD3A3u);
        check(!rt->stopped() && c.gpr[2] == expected && c.pc == 0x08001004u &&
              display->clock.ticks() == ticks && display->hcount_reads == reads + 1,
              "Current Hcount query advanced time or violated return ABI");
        for (unsigned i = 0; i < 32; ++i) if (i != 2)
            check(c.gpr[i] == before.gpr[i], "Current Hcount query clobbered register");
    };
    query(0);
    display->clock.advance(DisplayClock::ticks_per_hcount - 1); query(0);
    display->clock.advance(1); query(1);
    display->clock.advance(DisplayClock::ticks_per_frame - DisplayClock::ticks_per_hcount); query(0);
    check(threads->enter_interrupt(), "Hcount interrupt fixture"); query(0);
    threads->leave_interrupt();
}
void callback_wait_contracts() {
    using namespace azazel6;
    for (bool already_in_vblank : {false, true}) {
        auto rt = std::make_unique<psprecomp::Runtime>();
        auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
        auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
            0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
        auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
        auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
        auto display = register_display_hle(*rt, threads, delivery, 1);
        rt->register_function(0x08001008u, [](auto& runtime, auto&) { runtime.stop("Unnotified callback executed"); }, "inactive_callback");
        auto& c = rt->cpu();
        rt->memory().store8(0x08004000u, 0);
        c.gpr[4] = 0x08004000u; c.gpr[5] = 0x08001008u; c.gpr[6] = 0;
        rt->invoke_import("ThreadManForUser", 0xE81CAF8Fu, c);
        check(!rt->stopped() && threads->callbacks().size() == 1, "Inactive callback setup");
        if (already_in_vblank) display->advance(*rt, DisplayClock::ticks_per_frame);
        const auto frame = display->clock.frames();
        c.pc = 0x08001000u; c.gpr[31] = 0x08001004u; c.gpr[16] = 42; c.vfpu[127] = 19;
        rt->register_function(0x08001004u, [](auto& runtime, auto&) { runtime.stop("CB wait next edge"); }, "cb_wait_continuation");
        rt->invoke_import("sceDisplay", 0x46F186C3u, c);
        check(!rt->stopped() && threads->current_uid() == 0 && c.pc == kDisplayIdleGate &&
              display->clock.frames() == frame, "CB wait returned before next VBLANK");
        rt->run(c.pc, 4);
        check(rt->stop_reason() == "CB wait next edge" && threads->current_uid() == kBootstrapUid &&
              display->clock.frames() == frame + 1 && display->idle_frames() == 1 &&
              c.gpr[2] == 0 && c.gpr[16] == 42 && c.vfpu[127] == 19 && threads->callbacks().size() == 1,
              "CB wait lost context or dispatched an inactive callback");
    }
    for (bool interrupt : {false, true}) {
        auto rt = std::make_unique<psprecomp::Runtime>();
        auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
        auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
            0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
        auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
        auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
        auto display = register_display_hle(*rt, threads, delivery, 1);
        auto& c = rt->cpu();
        if (interrupt) check(threads->enter_interrupt(), "Enter interrupt fixture");
        else rt->invoke_import("Kernel_Library", 0x092968F4u, c);
        c.gpr[2] = 42;
        rt->invoke_import("sceDisplay", 0x46F186C3u, c);
        check(rt->stopped() && c.gpr[2] == 42 && threads->current_uid() == kBootstrapUid &&
              display->clock.frames() == 0, "CB wait accepted noninterruptible context");
    }
}
}
int main() {
    try {
        wait_contracts();
        framebuffer_readback_contracts();
        mixed_wait_contracts();
        short_timeout_budget();
        clock_context_boundaries();
        startup_delay_contracts();
        event_contracts(false);
        event_contracts(true);
        event_contracts(true, true);
        idle_boundaries();
        mode_contracts();
        callback_wait_contracts();
        system_time_contracts();
        hcount_contracts();
        using namespace azazel6;
        DisplayClock clock;
        constexpr auto frame = DisplayClock::ticks_per_frame;
        check(clock.advance(frame - 1).empty() && clock.frames() == 0, "Early display edge");
        check(clock.advance(1) == std::vector<std::uint64_t>{frame} && clock.frames() == 1, "Exact edge");
        check(clock.in_vblank(), "VBLANK start phase");
        check(clock.advance(0).empty(), "Duplicate zero advance");
        check(clock.advance(2 * frame) == std::vector<std::uint64_t>{2 * frame, 3 * frame}, "Event ordering");
        clock.advance(DisplayClock::vblank_ticks - 1);
        check(clock.in_vblank(), "VBLANK ended early");
        clock.advance(1); check(!clock.in_vblank(), "VBLANK end boundary");
        const auto before = clock.ticks();
        rejects([&] { clock.advance(std::numeric_limits<std::uint64_t>::max()); }, "Clock overflow accepted");
        rejects([&] { clock.advance(frame * 1025); }, "Event budget accepted");
        check(clock.ticks() == before, "Failed clock advance mutated time");
        DisplayClock long_run;
        for (int i = 0; i < 1000; ++i) long_run.advance(frame * 60);
        check(long_run.ticks() == DisplayClock::ticks_per_second * 1001 && long_run.frames() == 60000,
              "LCD rational timing drift");
        auto rt = std::make_unique<psprecomp::Runtime>();
        DisplayBuffers buffers;
        const Framebuffer first{0x04000000u, 512, 3}, second{0x04088000u, 512, 3};
        check(buffers.set(rt->memory(), first, 1) == 0 && buffers.pending() &&
              buffers.current().address == 0 && buffers.latched() == first, "Premature latch");
        buffers.vblank(); check(buffers.current() == first && !buffers.pending(), "VBLANK latch");
        check(buffers.set(rt->memory(), second, 1) == 0 && buffers.current() == first, "Next-frame base");
        check(buffers.set(rt->memory(), first, 0) == 0 && !buffers.pending(), "Immediate replacement");
        buffers.vblank(); check(buffers.current() == first, "Stale pending buffer resurrected");
        for (auto format : {0u, 1u, 2u, 3u}) {
            const Framebuffer f{0x08001000u, 512, format};
            check(buffers.set(rt->memory(), f, 1) == 0 && buffers.current().format == format,
                  "Immediate format/stride update");
            buffers.vblank(); check(buffers.current() == f, "RAM format latch");
        }
        auto snapshot = buffers.current();
        check(buffers.set(rt->memory(), {first.address + 1, 512, 3}, 1) == 0x80000103u, "Alignment error");
        check(buffers.set(rt->memory(), {first.address, 513, 3}, 1) == 0x80000104u, "Stride error");
        check(buffers.set(rt->memory(), {first.address, 512, 4}, 1) == 0x80000108u, "Format error");
        check(buffers.set(rt->memory(), first, 2) == 0x80000107u, "Sync error");
        for (auto address : {0x041FFFF0u, 0x09FFFFF0u, 0x44000000u, 0xFFFFFFF0u})
            rejects([&] { buffers.set(rt->memory(), {address, 512, 3}, 1); }, "Unchecked framebuffer span");
        rejects([&] { buffers.set(rt->memory(), {first.address, 0xFFFFFFC0u, 3}, 1); }, "Stride overflow");
        check(buffers.current() == snapshot && buffers.latched() == snapshot && !buffers.pending(),
              "Rejected framebuffer mutated state");
        check(buffers.set(rt->memory(), {0, 512, 3}, 1) == 0, "Display off");
        buffers.vblank(); check(buffers.current().address == 0, "Display off latch");
        DisplayBuffers isolated; check(isolated.current() == Framebuffer{}, "Display state isolation");
        std::cout << "Synthetic LCD clock and framebuffer contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
