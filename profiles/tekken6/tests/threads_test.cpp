#include "azazel6/threads.hpp"
#include "azazel6/bootstrap.hpp"
#include <iostream>
#include <stdexcept>

using namespace azazel6;
namespace {
constexpr ThreadArena arena{0x08010000u, 0x08011000u, 0x08001000u, 0x08010000u,
                            0x08001000u, 0x08002000u, 0x08003000u};
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Fixture {
    std::unique_ptr<psprecomp::Runtime> rt = std::make_unique<psprecomp::Runtime>();
    std::shared_ptr<ThreadRegistry> threads = register_thread_hle(*rt, arena);
    Fixture() {
        rt->memory().store8(0x08004000u, 'x');
        rt->memory().store8(0x08004001u, 0);
        for (auto address = arena.begin - 4; address <= arena.end; address += 4)
            rt->memory().store32(address, 0x11223344u);
    }
    void args(std::uint32_t size = 512, std::uint32_t attr = 0x80000000u) {
        auto& c = rt->cpu();
        c.gpr[2] = 0xDEADBEEFu; c.gpr[4] = 0x08004000u; c.gpr[5] = arena.executable_begin;
        c.gpr[6] = 40; c.gpr[7] = size; c.gpr[8] = attr; c.gpr[9] = 0;
        c.gpr[28] = 0; // GP must come from verified owning module, not caller's transient GP.
        c.gpr[31] = 0x08001800u; c.pc = 0x08001900u;
    }
    std::uint32_t create() {
        invoke_returning_import(*rt, rt->cpu(), "ThreadManForUser", kCreateThreadNid);
        return rt->cpu().gpr[2];
    }
    std::uint32_t erase(std::uint32_t uid) {
        rt->cpu().gpr[4] = uid;
        rt->invoke_import("ThreadManForUser", kDeleteThreadNid, rt->cpu());
        return rt->cpu().gpr[2];
    }
    void guards() {
        check(rt->memory().load32(arena.begin - 4) == 0x11223344u, "Image guard overwritten");
        check(rt->memory().load32(arena.end) == 0x11223344u, "Bootstrap guard overwritten");
    }
};
void ownership() {
    Fixture f; f.args(513, 0x80200000u);
    auto registers = f.rt->cpu().gpr;
    const auto first = f.create();
    check(first == 1 && f.rt->cpu().pc == registers[31], "Create return ABI");
    for (std::size_t i = 0; i < registers.size(); ++i)
        if (i != 2) check(f.rt->cpu().gpr[i] == registers[i], "Create clobbered register");
    auto record = f.threads->snapshot().at(0);
    check(record.name == "x" && record.stack_size == 768 && record.stack_base == arena.end - 768,
          "Name or rounded stack mismatch");
    check(record.gp == arena.module_gp && record.attributes == 0x802000FFu, "Module ownership/attributes");
    for (auto a = record.stack_base; a < arena.end; a += 4)
        check(f.rt->memory().load32(a) == 0xFFFFFFFFu, "Stack fill");
    f.args(512); const auto second = f.create();
    check(second > first && f.threads->snapshot().size() == 2, "Duplicate names/UID ownership");
    check(f.erase(first) == 0, "Delete dormant thread");
    for (auto a = record.stack_base; a < arena.end; a += 4)
        check(f.rt->memory().load32(a) == 0, "Clear-stack deletion");
    check(f.erase(first) == 0x80020198u, "Stale UID accepted");
    f.args(768, 0x80100000u); const auto third = f.create();
    check(third > second, "UID reused");
    auto last = f.threads->snapshot().back();
    check(last.stack_base == record.stack_base && f.rt->memory().load32(last.stack_base) == 0,
          "Freed hole not reused/no-fill violated");
    f.guards();
    Fixture other; other.args();
    check(other.create() == 1 && other.threads->snapshot().size() == 1, "Cross-runtime state leak");
    f.threads.reset(); f.args(); check(f.create() < 0x80000000u, "Callback state lifetime");
}
void failures() {
    for (const auto size : {0u, 511u, 0xFFFFFFFFu, 0xFFFF0000u, 4097u}) {
        Fixture f; f.args(size);
        check(f.create() == (size < 512 ? 0x80020194u : 0x80020190u), "Stack failure code");
        check(f.threads->snapshot().empty(), "Failed allocation created an object");
        check(f.rt->memory().load32(arena.end - 4) == 0x11223344u, "Failed allocation wrote memory");
        f.args(); check(f.create() == 1, "Failure consumed UID"); f.guards();
    }
    for (const auto priority : {0u, 7u, 120u, 0xFFFFFFFFu}) {
        Fixture f; f.args(); f.rt->cpu().gpr[6] = priority;
        check(f.create() == 0x80020193u && f.threads->snapshot().empty(), "Priority validation");
    }
    for (const auto priority : {8u, 119u}) {
        Fixture f; f.args(); f.rt->cpu().gpr[6] = priority; check(f.create() == 1, "Priority boundary");
    }
    Fixture f;
    for (unsigned i = 0; i < 8; ++i) { f.args(); check(f.create() == i + 1, "Pool fill"); }
    f.args(); check(f.create() == 0x80020190u && f.threads->snapshot().size() == 8, "Pool exhaustion");
    check(f.erase(4) == 0, "Interior delete"); f.args(); check(f.create() == 9, "Interior hole reuse");
    check(f.erase(2) == 0 && f.erase(3) == 0, "Adjacent frees");
    f.args(1024); check(f.create() == 10, "Adjacent holes failed to coalesce");
    f.guards();
}
void names_and_stops() {
    Fixture null_name; null_name.args(); null_name.rt->cpu().gpr[4] = 0;
    check(null_name.create() == 0x80020001u, "Null name");
    Fixture empty_name; empty_name.args(); empty_name.rt->memory().store8(0x08004000u, 0);
    check(empty_name.create() == 1 && empty_name.threads->snapshot()[0].name.empty(), "Empty name");
    Fixture long_name; long_name.args();
    for (unsigned i = 0; i < 40; ++i) long_name.rt->memory().store8(0x08004000u + i, 'q');
    check(long_name.create() == 1 && long_name.threads->snapshot()[0].name.size() == 31, "Name truncation");
    Fixture null_entry; null_entry.args(); null_entry.rt->cpu().gpr[5] = 0;
    check(null_entry.create() == 1 && null_entry.threads->snapshot()[0].entry == 0, "Dormant null entry");
    for (const auto [reg, value] : {std::pair{4u, 0xFFFFFFFFu}, {4u, 0x09FFFFFFu},
         {5u, 0x08010000u}, {5u, 0x08001001u}, {8u, 0x4000u}, {9u, 0x08004000u}}) {
        Fixture f; f.args(); f.rt->memory().store8(0x09FFFFFFu, 'x'); f.rt->cpu().gpr[reg] = value;
        check(f.create() == 0xDEADBEEFu && f.rt->stopped() && f.rt->cpu().pc == 0x08001900u,
              "Unsupported configuration returned success");
        check(f.threads->snapshot().empty(), "Unsupported configuration mutated registry"); f.guards();
    }
    Fixture f; f.args(); check(f.create() == 1, "Setup StartThread boundary");
    f.rt->invoke_import("ThreadManForUser", 0xF475845Du, f.rt->cpu());
    check(f.rt->stopped() && f.threads->snapshot().size() == 1, "StartThread must remain unsupported");
}
void arena_validation() {
    for (int variant = 0; variant < 5; ++variant) {
        auto rt = std::make_unique<psprecomp::Runtime>(); auto invalid = arena;
        if (variant == 0) invalid.begin -= 256;
        if (variant == 1) invalid.end = 0xFFFFFFFFu;
        if (variant == 2) invalid.begin += 1;
        if (variant == 3) invalid.executable_end = invalid.image_end + 4;
        if (variant == 4) invalid.module_gp = invalid.image_end;
        bool rejected = false;
        try { (void)register_thread_hle(*rt, invalid); } catch (const std::exception&) { rejected = true; }
        check(rejected, "Invalid ownership accepted");
    }
}
struct Scheduled {
    std::unique_ptr<psprecomp::Runtime> rt = std::make_unique<psprecomp::Runtime>();
    std::shared_ptr<ThreadRegistry> threads;
    Scheduled() {
        auto config = arena; config.bootstrap_priority = 32; config.bootstrap_stack_size = 512;
        threads = register_thread_hle(*rt, config);
        rt->register_function(arena.executable_begin, [](auto&, auto& c) { c.pc = c.gpr[31]; }, "synthetic");
        rt->memory().store8(0x08004000u, 0);
    }
    std::uint32_t create(std::uint32_t priority) {
        auto& c = rt->cpu(); c.gpr[4] = 0x08004000u; c.gpr[5] = arena.executable_begin;
        c.gpr[6] = priority; c.gpr[7] = 1024; c.gpr[8] = 0; c.gpr[9] = 0;
        rt->invoke_import("ThreadManForUser", kCreateThreadNid, c); return c.gpr[2];
    }
    void start(std::uint32_t uid, std::uint32_t size = 0, std::uint32_t ptr = 0) {
        auto& c = rt->cpu(); c.pc = 0x08001900u; c.gpr[31] = 0x08001800u;
        c.gpr[4] = uid; c.gpr[5] = size; c.gpr[6] = ptr;
        invoke_returning_import(*rt, c, "ThreadManForUser", kStartThreadNid);
    }
    void exit() { rt->invoke_import("ThreadManForUser", 0xAA73C935u, rt->cpu()); }
};
void scheduling() {
    Scheduled f;
    const auto high = f.create(8);
    f.rt->cpu().fpr[7] = 7.25f; f.rt->cpu().vfpu[83] = 9.5f; f.rt->cpu().hi = 42;
    f.start(high);
    check(f.threads->current_uid() == high && f.rt->cpu().pc == arena.executable_begin, "Priority preemption");
    check(f.rt->cpu().gpr[28] == arena.module_gp && f.rt->cpu().gpr[31] == kThreadReturnGate, "New thread ABI");
    f.exit();
    check(f.threads->current_uid() == kBootstrapUid && f.rt->cpu().pc == 0x08001800u, "Caller continuation");
    check(f.rt->cpu().gpr[2] == 0 && f.rt->cpu().fpr[7] == 7.25f &&
          f.rt->cpu().vfpu[83] == 9.5f && f.rt->cpu().hi == 42, "Saved full CPU context");
    for (auto priority : {32u, 40u}) {
        const auto uid = f.create(priority); f.start(uid);
        check(f.threads->current_uid() == kBootstrapUid && f.rt->cpu().pc == 0x08001800u, "Equal/lower priority preempted");
        f.start(uid); check(f.rt->cpu().gpr[2] == 0x800201A4u, "Double start accepted");
        f.rt->cpu().gpr[4] = uid;
        f.rt->invoke_import("ThreadManForUser", kDeleteThreadNid, f.rt->cpu());
        check(f.rt->cpu().gpr[2] == 0x800201A4u, "Ready thread deleted");
    }
    f.exit(); check(f.threads->snapshot()[1].status == ThreadStatus::Running, "Highest ready thread not selected");
    f.rt->cpu().gpr[2] = 123;
    f.rt->invoke_import("Azazel6Host", 1, f.rt->cpu());
    check(f.threads->snapshot()[1].exit_status == 123 && f.threads->snapshot()[2].status == ThreadStatus::Running,
          "Return gate status/handoff");
    f.rt->invoke_import("ThreadManForUser", 0x809CE29Bu, f.rt->cpu());
    check(f.rt->stopped() && f.threads->snapshot().size() == 2, "Exit-delete/no runnable boundary");
}
void start_arguments() {
    Scheduled f; const auto uid = f.create(40);
    const auto bottom = f.threads->snapshot()[0].stack_base;
    for (unsigned i = 0; i < 9; ++i) f.rt->memory().store8(bottom + 32 + i, static_cast<std::uint8_t>(i + 1));
    f.start(uid, 9, bottom + 32); // Aliases the stack that initialization will overwrite.
    const auto thread = f.threads->snapshot()[0]; const auto& c = thread.context;
    check(c.gpr[4] == 9 && c.gpr[5] == bottom + 1024 - 256 - 16, "Argument placement");
    check(c.gpr[29] == c.gpr[5] - 64 && c.gpr[26] == bottom + 768, "SP/TLS layout");
    for (unsigned i = 0; i < 9; ++i) check(f.rt->memory().load8(c.gpr[5] + i) == i + 1, "Aliased argument copy");
    check(f.rt->memory().load32(bottom) == uid && f.rt->memory().load32(c.gpr[26] + 192) == uid,
          "Stack UID ownership");
    Scheduled null_args; const auto n = null_args.create(40); null_args.start(n, 8, 0);
    check(null_args.threads->snapshot()[0].context.gpr[4] == 0, "Null pointer must suppress arguments");
    Scheduled bad; const auto b = bad.create(40); bad.start(b, 4, 0xFFFFFFFEu);
    check(bad.rt->cpu().gpr[2] == 0x800200D3u && bad.threads->snapshot()[0].status == ThreadStatus::Dormant,
          "Argument overflow accepted");
    bad.start(b, 0xFFFFFFFFu, 0); check(bad.rt->cpu().gpr[2] == 0x800200D3u, "Negative size accepted");
    Scheduled large; const auto l = large.create(40); large.start(l, 900, 0x08004000u);
    check(large.rt->stopped() && large.threads->snapshot()[0].status == ThreadStatus::Dormant, "Oversized argument mutation");
}
}
int main() {
    try { ownership(); failures(); names_and_stops(); arena_validation(); scheduling(); start_arguments();
          std::cout << "Dormant thread ownership and failure contracts passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
