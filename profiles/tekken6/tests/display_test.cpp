#include "azazel6/display.hpp"
#include "azazel6/threads.hpp"
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
}
int main() {
    try {
        wait_contracts();
        using namespace azazel6;
        DisplayClock clock;
        constexpr auto frame = DisplayClock::ticks_per_frame;
        check(clock.advance(frame - 1).empty() && clock.frames() == 0, "Early display edge");
        check(clock.advance(1) == std::vector<std::uint64_t>{frame} && clock.frames() == 1, "Exact edge");
        check(clock.advance(0).empty(), "Duplicate zero advance");
        check(clock.advance(2 * frame) == std::vector<std::uint64_t>{2 * frame, 3 * frame}, "Event ordering");
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
