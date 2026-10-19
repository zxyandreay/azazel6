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
};
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
    check(wait.call(wait_nid, waiting, 2, 0x20, output) == 0xDEADBEEFu && wait.rt->stopped() &&
        wait.threads->event_flags().at(waiting).pattern == 5 && wait.rt->memory().load32(output) == 0x11223344u,
        "Unsatisfied wait mutated state");
    for (auto nid : {create_nid, delete_nid, set_nid, clear_nid, poll_nid, wait_nid}) {
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
    try { lifecycle(); matching(); rejecting_boundaries(); std::cout << "Immediate event flag contracts passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
