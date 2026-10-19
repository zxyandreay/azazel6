#include "azazel6/interrupts.hpp"
#include "azazel6/bootstrap.hpp"
#include <iostream>
#include <stdexcept>

namespace {
constexpr std::uint32_t begin = 0x08001000u, end = 0x08002000u;
constexpr auto reg = 0xCA04A2B9u, release = 0xD61E6961u, enable = 0xFB8E22ECu, disable = 0x8A389411u;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Fixture {
    std::unique_ptr<psprecomp::Runtime> rt = std::make_unique<psprecomp::Runtime>();
    std::shared_ptr<azazel6::InterruptState> state = azazel6::register_interrupt_hle(*rt, begin, end, end);
    Fixture() { rt->register_function(begin, [](auto&, auto&) {}, "synthetic_interrupt"); }
    std::uint32_t call(std::uint32_t nid, std::uint32_t function = begin,
                       std::uint32_t intr = 30, std::uint32_t sub = 0) {
        auto& c = rt->cpu(); c.gpr[4] = intr; c.gpr[5] = sub; c.gpr[6] = function;
        c.gpr[7] = 0xFFFFFFFFu; c.gpr[2] = 0xDEADBEEFu; c.pc = begin; c.gpr[31] = end;
        azazel6::invoke_returning_import(*rt, c, "InterruptManager", nid); return c.gpr[2];
    }
};
struct DeliveryFixture {
    std::unique_ptr<psprecomp::Runtime> rt = std::make_unique<psprecomp::Runtime>();
    std::shared_ptr<azazel6::GuestAllocator> allocator =
        std::make_shared<azazel6::GuestAllocator>(0x08010000u, 0x08020000u);
    std::shared_ptr<azazel6::ThreadRegistry> threads = azazel6::register_thread_hle(*rt,
        {0x08010000u, 0x08020000u, begin, 0x08010000u, begin, end, end, 32, 4096}, allocator);
    std::shared_ptr<azazel6::InterruptState> state = azazel6::register_interrupt_hle(*rt, begin, end, end);
    std::shared_ptr<azazel6::InterruptDelivery> delivery =
        azazel6::register_interrupt_delivery(*rt, state, threads, allocator);
    DeliveryFixture() {
        rt->register_function(begin, [](auto&, auto&) {}, "synthetic_interrupt");
        call(reg); call(enable);
    }
    void call(std::uint32_t nid) {
        auto& c = rt->cpu(); c.gpr[4] = 30; c.gpr[5] = 0; c.gpr[6] = begin; c.gpr[7] = 0x08004000u;
        rt->invoke_import("InterruptManager", nid, c);
    }
    void mask(bool enable_mask) {
        auto& c = rt->cpu(); c.gpr[4] = enable_mask ? 1 : 0;
        rt->invoke_import("Kernel_Library", enable_mask ? 0x5F10D406u : 0x092968F4u, c);
    }
    void finish() {
        rt->cpu().pc = azazel6::kInterruptReturnGate;
        rt->invoke_import("Azazel6Host", 2, rt->cpu());
    }
};
void delivery_contracts() {
    DeliveryFixture f;
    auto& c = f.rt->cpu();
    f.mask(false); f.delivery->raise_vblank();
    check(f.delivery->pending() == 0 && !f.delivery->dispatch(*f.rt, c), "Masked edge must be dropped");
    f.mask(true); f.delivery->raise_vblank(); f.mask(false);
    check(!f.delivery->dispatch(*f.rt, c) && f.delivery->pending() == 1, "Pending delivery bypassed mask");
    f.mask(true);
    for (std::size_t i = 0; i < c.gpr.size(); ++i) c.gpr[i] = static_cast<std::uint32_t>(i * 123);
    for (std::size_t i = 0; i < c.fpr.size(); ++i) c.set_fpr_bits(static_cast<std::uint32_t>(i), 0x7FC00000u + static_cast<std::uint32_t>(i));
    for (std::size_t i = 0; i < c.vfpu.size(); ++i) c.vfpu[i] = static_cast<float>(i);
    for (std::size_t i = 0; i < c.vfpu_ctrl.size(); ++i) c.vfpu_ctrl[i] = static_cast<std::uint32_t>(i * 99);
    c.pc = end - 4; c.hi = 1234; c.lo = 5678; c.fcr31 = 0xDEADBEEFu;
    const auto saved = c;
    check(f.delivery->dispatch(*f.rt, c) && f.threads->in_interrupt() &&
          c.pc == begin && c.gpr[4] == 0 && c.gpr[5] == 0x08004000u && c.gpr[28] == end &&
          c.gpr[31] == azazel6::kInterruptReturnGate && c.gpr[29] >= 0x08010000u &&
          c.gpr[29] < 0x08020000u && f.allocator->total_free() == 0xF000u, "Owned callback ABI/stack");
    check(!f.delivery->dispatch(*f.rt, c), "Nested interrupt");
    c = {}; f.finish();
    check(c.gpr == saved.gpr && c.pc == saved.pc && c.hi == saved.hi && c.lo == saved.lo &&
          c.fcr31 == saved.fcr31 && c.vfpu == saved.vfpu && c.vfpu_ctrl == saved.vfpu_ctrl,
          "Full interrupt context restoration");
    for (std::uint32_t i = 0; i < 32; ++i) check(c.fpr_bits(i) == saved.fpr_bits(i), "FPR payload restoration");
    check(!f.delivery->active() && !f.threads->in_interrupt() && f.threads->interrupts_enabled(), "Interrupt return state");
    f.delivery->raise_vblank(); f.call(release); f.call(reg); f.call(enable);
    check(!f.delivery->dispatch(*f.rt, c) && f.delivery->pending() == 0, "Stale event invoked replacement registration");
    for (int i = 0; i < 1024; ++i) f.delivery->raise_vblank();
    bool caught = false;
    try { f.delivery->raise_vblank(); } catch (const std::exception&) { caught = true; }
    check(caught && f.delivery->pending() == 1024, "Unbounded event queue");
    DeliveryFixture mutation;
    mutation.delivery->raise_vblank(); mutation.delivery->dispatch(*mutation.rt, mutation.rt->cpu());
    mutation.mask(true);
    check(mutation.rt->stopped(), "Handler enabled thread mutations");
    DeliveryFixture uncovered;
    uncovered.delivery->raise_vblank(); uncovered.rt->unregister_functions(begin, end);
    check(!uncovered.delivery->dispatch(*uncovered.rt, uncovered.rt->cpu()) && uncovered.rt->stopped(), "Removed AOT handler dispatched");
}
}
int main() {
    try {
        delivery_contracts();
        Fixture f;
        check(f.call(release) == 0x80020068u, "Release missing registration");
        check(f.call(reg) == 0 && !f.state->enabled && f.state->generation == 1 &&
              f.state->vblank->argument == 0xFFFFFFFFu && f.state->vblank->gp == end, "Registration ABI");
        check(f.call(reg) == 0x80020067u && f.state->generation == 1, "Duplicate registration");
        check(f.call(enable) == 0 && f.state->enabled && f.call(enable) == 0, "Enable idempotency");
        check(f.call(disable) == 0 && !f.state->enabled && f.call(disable) == 0, "Disable idempotency");
        check(f.call(enable) == 0 && f.call(release) == 0 && !f.state->enabled && !f.state->vblank,
              "Release ownership");
        check(f.call(reg) == 0 && f.state->generation == 2 && f.rt->cpu().pc == end, "Reuse/return ABI");
        Fixture isolated; check(!isolated.state->vblank, "Runtime isolation");
        check(isolated.call(enable) == 0 && isolated.call(reg) == 0 && isolated.state->enabled,
              "Enable before registration was lost");
        for (auto function : {0u, begin + 1, begin + 4, end, 0xFFFFFFFFu, 0x88001000u}) {
            Fixture bad;
            check(bad.call(reg, function) == 0xDEADBEEFu && bad.rt->stopped() && !bad.state->vblank &&
                  bad.state->generation == 0 && bad.rt->cpu().pc == begin, "Bad pointer or uncovered AOT accepted");
        }
        for (auto intr : {68u, 0xFFFFFFFFu}) {
            Fixture bad; check(bad.call(reg, begin, intr) == 0x80020065u && !bad.state->vblank, "Invalid interrupt");
        }
        Fixture other; check(other.call(reg, begin, 29) == 0xDEADBEEFu && other.rt->stopped(), "Unsupported controller");
        Fixture sub; check(sub.call(reg, begin, 30, 1) == 0xDEADBEEFu && sub.rt->stopped(), "Unsupported slot");
        std::cout << "VBLANK registration contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
