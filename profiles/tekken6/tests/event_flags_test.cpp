#include "azazel6/threads.hpp"
#include "azazel6/bootstrap.hpp"
#include <iostream>
#include <stdexcept>
using namespace azazel6;
namespace {
constexpr std::uint32_t name = 0x08003000u, output = 0x08004000u;
constexpr std::uint32_t create_nid = 0x55C20A00u, delete_nid = 0xEF9E4C70u;
constexpr std::uint32_t set_nid = 0x1FB15A32u, clear_nid = 0x812346E4u;
constexpr std::uint32_t poll_nid = 0x30FD48F0u, wait_nid = 0x402FCF22u;
constexpr std::uint32_t cancel_nid = 0xCD203292u;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Fixture {
    std::unique_ptr<psprecomp::Runtime> rt = std::make_unique<psprecomp::Runtime>();
    std::shared_ptr<ThreadRegistry> threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u,
        0x08001000u, 0x08010000u, 0x08001000u, 0x08002000u, name, 32, 512});
    Fixture() { rt->memory().store8(name, 'e'); rt->memory().store8(name + 1, 0); }
    std::uint32_t call(std::uint32_t nid, std::uint32_t a0, std::uint32_t a1 = 0,
                       std::uint32_t a2 = 0, std::uint32_t a3 = 0, std::uint32_t a4 = 0) {
        auto& c = rt->cpu(); c.gpr[4] = a0; c.gpr[5] = a1; c.gpr[6] = a2; c.gpr[7] = a3; c.gpr[8] = a4;
        c.gpr[2] = 0xDEADBEEFu; c.pc = 0x08001900u; c.gpr[31] = 0x08001800u;
        invoke_returning_import(*rt, c, "ThreadManForUser", nid); return c.gpr[2];
    }
    std::uint32_t create(std::uint32_t pattern = 5, std::uint32_t attr = 0x200) {
        return call(create_nid, name, attr, pattern);
    }
    std::uint32_t worker(std::uint32_t priority) {
        rt->register_function(0x08001000u, [](auto&, auto&) {}, "synthetic_worker");
        rt->cpu().gpr[9] = 0;
        const auto uid = call(kCreateThreadNid, name, 0x08001000u, priority, 512);
        call(kStartThreadNid, uid);
        check(!rt->stopped(), "Worker creation/start");
        return uid;
    }
};
void queued_waits() {
    Fixture f; const auto uid = f.create(0);
    const auto first = f.worker(40), second = f.worker(48);
    f.rt->memory().store32(output, 0x11223344u);
    f.rt->cpu().gpr[16] = 42; f.rt->cpu().vfpu[127] = 123;
    f.call(wait_nid, uid, 1, 0x20, output);
    check(f.threads->current_uid() == first && f.rt->memory().load32(output) == 0x11223344u,
          "Wait must switch without writing output");
    f.call(wait_nid, uid, 1, 0, output + 4);
    check(f.threads->current_uid() == second, "Second waiter did not yield");
    f.call(set_nid, uid, 1);
    check(f.threads->current_uid() == kBootstrapUid && f.rt->cpu().gpr[2] == 0 &&
          f.rt->cpu().gpr[16] == 42 && f.rt->cpu().vfpu[127] == 123 &&
          f.rt->cpu().pc == 0x08001800u && f.rt->memory().load32(output) == 1 &&
          f.threads->event_flags().at(uid).pattern == 0 &&
          f.threads->event_flags().at(uid).waiters.size() == 1,
          "FIFO clear/preemption/context restoration");
    f.call(delete_nid, uid);
    const auto threads = f.threads->snapshot();
    check(threads[0].uid == first && threads[0].status == ThreadStatus::Ready &&
          !threads[0].event_wait && threads[0].context.gpr[2] == 0x800201B5u &&
          f.rt->memory().load32(output + 4) == 0, "Delete did not release retained waiter");

    Fixture cancel; const auto event = cancel.create(0); cancel.worker(40);
    cancel.call(wait_nid, event, 1, 0, output);
    cancel.call(cancel_nid, event, 7, output + 4);
    check(cancel.threads->current_uid() == kBootstrapUid && cancel.rt->cpu().gpr[2] == 0x800201A9u &&
          cancel.rt->memory().load32(output) == 7 && cancel.rt->memory().load32(output + 4) == 1 &&
          cancel.threads->event_flags().at(event).pattern == 7 &&
          cancel.threads->event_flags().at(event).waiters.empty(), "Cancellation pattern/count/result");

    Fixture single; const auto one = single.create(0, 0); single.worker(40);
    single.call(wait_nid, one, 1);
    check(single.call(poll_nid, one, 1, 0, output) == 0x800201B0u &&
          single.rt->memory().load32(output) == 0, "Single-waiter poll error/output");
    check(single.call(wait_nid, one, 1) == 0x800201B0u &&
          single.threads->event_flags().at(one).waiters.size() == 1, "Single-waiter attribute");
    single.call(set_nid, one, 1);
    check(single.threads->current_uid() == kBootstrapUid, "Single waiter wake");

    Fixture invalid; const auto bad = invalid.create(0); invalid.worker(40);
    invalid.call(wait_nid, bad, 1, 0, output);
    invalid.rt->register_function(output, [](auto&, auto&) {}, "new_code_ownership");
    invalid.call(set_nid, bad, 1);
    check(invalid.rt->stopped() && invalid.threads->event_flags().at(bad).pattern == 0 &&
          invalid.threads->event_flags().at(bad).waiters.size() == 1,
          "Retained output revalidation must precede mutation");
}
void lifecycle() {
    Fixture f; const auto first = f.create(0x80000005u);
    check(first == 0x70000000u && f.rt->cpu().pc == 0x08001800u, "Create UID/ABI");
    check(f.threads->event_flags().at(first).initial == 0x80000005u &&
        f.threads->event_flags().at(first).attributes == 0x200, "Initial pattern/attributes");
    check(f.call(set_nid, first, 2) == 0 && f.threads->event_flags().at(first).pattern == 0x80000007u, "Set must OR");
    check(f.call(clear_nid, first, 6) == 0 && f.threads->event_flags().at(first).pattern == 6, "Clear must AND");
    f.call(set_nid, first, 0); f.call(clear_nid, first, 0xFFFFFFFFu);
    check(f.threads->event_flags().at(first).pattern == 6, "Zero set/full retain mask");
    f.call(clear_nid, first, 0); check(f.threads->event_flags().at(first).pattern == 0, "Clear all");
    const auto second = f.create(); check(second != first, "Duplicate name rejected");
    for (std::uint32_t i = 0; i < 31; ++i) f.rt->memory().store8(name + i, 'n');
    const auto long_name = f.create(); f.rt->memory().store8(name, 0);
    check(f.threads->event_flags().at(long_name).name == std::string(31, 'n'), "Name not copied/bounded");
    check(f.call(delete_nid, first) == 0 && f.threads->event_flags().size() == 2, "Deletion changed unrelated objects");
    for (auto nid : {delete_nid, set_nid, clear_nid, poll_nid, wait_nid}) {
        check(f.call(nid, first, 1) == 0x8002019Au, "Deleted UID accepted");
        check(f.call(nid, 0x60000000u, 1) == 0x8002019Au, "Cross-type UID accepted");
    }
    Fixture other; check(other.call(set_nid, second, 1) == 0x8002019Au, "Cross-runtime event leak");
}
void matching() {
    for (auto mode : {0u, 1u, 0x10u, 0x11u, 0x20u, 0x21u}) {
        Fixture f; const auto uid = f.create(0x80000005u);
        const auto bits = mode & 1u ? 3u : 5u;
        check(f.call(poll_nid, uid, bits, mode, output) == 0 &&
            f.rt->memory().load32(output) == 0x80000005u, "Match/complete pre-clear pattern");
        const auto expected = mode & 0x10u ? 0u : mode & 0x20u ? 0x80000005u & ~bits : 0x80000005u;
        check(f.threads->event_flags().at(uid).pattern == expected, "Clear mode");
    }
    Fixture f; const auto uid = f.create();
    check(f.call(wait_nid, uid, 1, 0x30, output) == 0 &&
          f.threads->event_flags().at(uid).pattern == 0 && f.rt->memory().load32(output) == 5,
          "Wait accepts combined clear modes with clear-all result");
    f.call(set_nid, uid, 5);
    check(f.call(wait_nid, uid, 5, 0x20, 0) == 0 && f.threads->event_flags().at(uid).pattern == 0,
        "Immediate wait/null result");
    f.call(set_nid, uid, 4);
    for (auto mode : {0u, 1u, 0x10u, 0x20u}) {
        f.rt->memory().store32(output, 0xDEADBEEFu);
        check(f.call(poll_nid, uid, 2, mode, output) == 0x800201AFu &&
            f.rt->memory().load32(output) == 4 && f.threads->event_flags().at(uid).pattern == 4,
            "Failed poll must report pattern without clearing");
    }
    for (auto mode : {2u, 4u, 0x30u, 0x31u, 0xFFFFFFFFu}) {
        f.rt->memory().store32(output, 0x11223344u);
        check(f.call(poll_nid, 0, 0, mode, output) == 0x80020195u &&
            f.rt->memory().load32(output) == 0x11223344u, "Invalid mode precedence/output");
    }
    check(f.call(poll_nid, 0, 0) == 0x800201B1u, "Zero pattern precedence");
    const auto registers = f.rt->cpu().gpr;
    f.rt->invoke_import("ThreadManForUser", poll_nid, f.rt->cpu());
    check(f.rt->cpu().gpr == registers, "Poll clobbered registers");
}
void rejecting_boundaries() {
    for (auto bad : {output + 1, 0xFFFFFFFFu, 0x88004000u, 0x08001000u, 0x0A000000u}) {
        Fixture f; const auto uid = f.create();
        check(f.call(poll_nid, uid, 5, 0x10, bad) == 0xDEADBEEFu && f.rt->stopped() &&
            f.threads->event_flags().at(uid).pattern == 5 && f.rt->cpu().pc == 0x08001900u,
            "Bad result address cleared event or returned");
    }
    Fixture timeout; const auto timed = timeout.create();
    check(timeout.call(wait_nid, timed, 5, 0x10, output, output + 4) == 0xDEADBEEFu &&
        timeout.rt->stopped() && timeout.threads->event_flags().at(timed).pattern == 5, "Unsupported timeout mutated event");
    Fixture wait; const auto waiting = wait.create(); wait.rt->memory().store32(output, 0x11223344u);
    wait.call(wait_nid, waiting, 2, 0x20, output);
    check(!wait.rt->stopped() && wait.threads->current_uid() == 0 &&
        wait.rt->cpu().pc == kDisplayIdleGate && wait.threads->event_flags().at(waiting).waiters.size() == 1 &&
        wait.threads->event_flags().at(waiting).pattern == 5 && wait.rt->memory().load32(output) == 0x11223344u,
        "Unsatisfied wait did not retain context/output");
    for (auto nid : {create_nid, delete_nid, set_nid, clear_nid, poll_nid, wait_nid, cancel_nid}) {
        Fixture f; const auto uid = f.create(); f.rt->invoke_import("Kernel_Library", 0x092968F4u, f.rt->cpu());
        check(f.call(nid, uid, 5) == 0xDEADBEEFu && f.rt->stopped() &&
            f.threads->event_flags().size() == 1 && f.threads->event_flags().at(uid).pattern == 5,
            "Disabled interrupts allowed mutation");
    }
    Fixture null_name; check(null_name.call(create_nid, 0) == 0x80020001u, "Null name error");
    Fixture attr; check(attr.create(0, 0x100) == 0x80020191u && attr.threads->event_flags().empty(), "Invalid attributes");
    Fixture unknown; unknown.create(0, 1); check(unknown.rt->stopped(), "Unmodeled valid attributes accepted");
    Fixture options; options.call(create_nid, name, 0, 0, output); check(options.rt->stopped(), "Unknown options accepted");
    Fixture names; names.call(create_nid, 0xFFFFFFFFu); check(names.rt->stopped(), "Invalid name accepted");
    Fixture capacity;
    for (unsigned i = 0; i < 4096; ++i) check(capacity.create() == 0x70000000u + i, "Premature object exhaustion");
    check(capacity.create() == 0x80020190u && capacity.threads->event_flags().size() == 4096, "Capacity bound");
}
}
int main() {
    try { lifecycle(); matching(); queued_waits(); rejecting_boundaries(); std::cout << "Event flag scheduler contracts passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
