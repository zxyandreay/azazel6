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
void interrupt_state() {
    Scheduled f;
    auto& c = f.rt->cpu();
    auto call = [&](std::uint32_t nid, std::uint32_t arg = 0) {
        c.pc = 0x08001900u; c.gpr[31] = 0x08001800u; c.gpr[4] = arg;
        invoke_returning_import(*f.rt, c, "Kernel_Library", nid);
    };
    call(0x092968F4u); check(c.gpr[2] == 1 && !f.threads->interrupts_enabled(), "Suspend previous state");
    call(0x092968F4u); check(c.gpr[2] == 0, "Nested suspend token");
    c.gpr[2] = 0x12345678u; call(0x5F10D406u, 0);
    check(!f.threads->interrupts_enabled() && c.gpr[2] == 0x12345678u, "Nested restore/void ABI");
    call(0x3B84732Du, 1);
    check(f.threads->interrupts_enabled() && c.pc == c.gpr[31], "Synchronized restore return");
    call(0x5F10D406u, 0xFFFFFFFFu);
    check(f.rt->stopped() && f.threads->interrupts_enabled() && c.pc == 0x08001900u, "Unsupported flags mutated state");
    Scheduled other; const auto uid = other.create(8);
    other.rt->invoke_import("Kernel_Library", 0x092968F4u, other.rt->cpu());
    other.start(uid);
    check(other.rt->stopped() && other.threads->current_uid() == kBootstrapUid &&
          other.threads->snapshot()[0].status == ThreadStatus::Dormant, "Disabled-interrupt scheduling accepted");
    Scheduled independent; check(independent.threads->interrupts_enabled(), "Interrupt state leaked across runtimes");
}
std::uint32_t mutex_call(Scheduled& f, std::uint32_t nid, std::uint32_t a0,
                         std::uint32_t a1 = 0, std::uint32_t a2 = 0, std::uint32_t a3 = 0) {
    auto& c = f.rt->cpu(); c.gpr[4] = a0; c.gpr[5] = a1; c.gpr[6] = a2; c.gpr[7] = a3;
    f.rt->invoke_import("ThreadManForUser", nid, c); return c.gpr[2];
}
void mutex_ownership() {
    Scheduled f;
    const auto uid = mutex_call(f, 0xB7D098C6u, 0x08004000u);
    check(uid == 0x30000000u && f.threads->mutexes().at(uid).owner == 0, "Mutex creation namespace/ownership");
    check(mutex_call(f, 0xB011B11Fu, uid, 0) == 0x800201BDu, "Zero lock count");
    check(mutex_call(f, 0xB011B11Fu, uid, 1) == 0, "First lock");
    check(mutex_call(f, 0xB011B11Fu, uid, 1) == 0x800201C8u, "Nonrecursive relock");
    const auto high = f.create(8); f.start(high);
    check(mutex_call(f, 0x0DDCD2C9u, uid, 1) == 0x800201C4u, "Try-lock by other owner");
    check(mutex_call(f, 0x6B30100Fu, uid, 1) == 0x800201C5u, "Unlock by other owner");
    const auto child_mutex = mutex_call(f, 0xB7D098C6u, 0x08004000u, 0, 1);
    check(f.threads->mutexes().at(child_mutex).owner == high, "Initial owner");
    f.exit();
    check(f.threads->mutexes().at(child_mutex).owner == 0 && f.threads->mutexes().at(child_mutex).count == 0,
          "Exiting owner retained mutex");
    check(mutex_call(f, 0x6B30100Fu, uid, 1) == 0 && mutex_call(f, 0x6B30100Fu, uid, 1) == 0x800201C5u,
          "Unlock ownership transition");
    check(mutex_call(f, 0xF8170FBEu, uid) == 0 && mutex_call(f, 0xF8170FBEu, uid) == 0x800201C3u,
          "Delete/stale UID");
    check(mutex_call(f, 0xB011B11Fu, uid, 1) == 0x800201C3u, "Deleted mutex locked");
    check(mutex_call(f, 0xB7D098C6u, 0x08004000u) != uid, "Deleted UID reused");
    const auto recursive = mutex_call(f, 0xB7D098C6u, 0x08004000u, 0x200, 1);
    check(mutex_call(f, 0xB011B11Fu, recursive, 0x7FFFFFFEu) == 0, "Recursive count");
    check(mutex_call(f, 0xB011B11Fu, recursive, 1) == 0x800201C6u, "Recursive overflow");
    check(mutex_call(f, 0x6B30100Fu, recursive, 0x7FFFFFFEu) == 0, "Recursive unlock");
    check(mutex_call(f, 0x6B30100Fu, recursive, 2) == 0x800201C7u, "Recursive underflow");
    check(mutex_call(f, 0xB7D098C6u, 0) == 0x80020001u, "Null mutex name");
    check(mutex_call(f, 0xB7D098C6u, 0x08004000u, 0, 2) == 0x800201BDu, "Invalid initial count");
    check(mutex_call(f, 0xB7D098C6u, 0x08004000u, 0x400) == 0x80020191u, "Invalid mutex attribute");
    for (unsigned i = 0; i < 40; ++i) f.rt->memory().store8(0x08004000u + i, 'X');
    const auto named = mutex_call(f, 0xB7D098C6u, 0x08004000u);
    check(f.threads->mutexes().at(named).name.size() == 31, "Unbounded name");
}
void current_priority() {
    Scheduled f;
    const auto query = [&](std::uint32_t expected) {
        auto& c = f.rt->cpu(); c.pc = 0x08001900u; c.gpr[31] = 0x08001800u;
        const auto before = c.gpr;
        invoke_returning_import(*f.rt, c, "ThreadManForUser", 0x94AA61EEu);
        check(!f.rt->stopped() && c.gpr[2] == expected && c.pc == before[31], "Current priority query/return");
        for (std::size_t i = 0; i < before.size(); ++i)
            if (i != 2) check(c.gpr[i] == before[i], "Priority query clobbered registers");
    };
    query(32);
    const auto high = f.create(8); f.start(high); query(8);
    f.exit(); query(32);
    const auto low = f.create(119); f.start(low); query(32);
    f.exit(); query(119);
    f.rt->invoke_import("Kernel_Library", 0x092968F4u, f.rt->cpu()); query(119);
    Fixture inactive; inactive.args();
    invoke_returning_import(*inactive.rt, inactive.rt->cpu(), "ThreadManForUser", 0x94AA61EEu);
    check(inactive.rt->stopped() && inactive.rt->cpu().gpr[2] == 0xDEADBEEFu &&
          inactive.rt->cpu().pc == 0x08001900u, "Missing running context fabricated a priority");
}
void priority_changes() {
    const auto change = [](Scheduled& f, std::uint32_t uid, std::uint32_t priority) {
        auto& c = f.rt->cpu(); c.gpr[4] = uid; c.gpr[5] = priority; c.gpr[2] = 0xDEADBEEFu;
        c.pc = 0x08001900u; c.gpr[31] = 0x08001800u;
        invoke_returning_import(*f.rt, c, "ThreadManForUser", 0x71BC9871u);
    };
    Scheduled f; const auto low = f.create(40);
    change(f, low, 19); check(f.rt->cpu().gpr[2] == 0x800201A2u, "Dormant priority mutation");
    change(f, 0xFFFFFFFFu, 19); check(f.rt->cpu().gpr[2] == 0x80020198u, "Invalid priority UID");
    f.start(low);
    for (auto p : {1u, 7u, 120u, 0xFFFFFFFFu}) {
        change(f, low, p);
        check(f.rt->cpu().gpr[2] == 0x80020193u && f.threads->snapshot()[0].priority == 40,
              "Invalid priority mutated ready thread");
    }
    change(f, low, 8);
    check(f.threads->current_uid() == low && f.rt->cpu().pc == arena.executable_begin,
          "Raising ready thread did not preempt");
    change(f, 0, 119);
    check(f.threads->current_uid() == kBootstrapUid && f.rt->cpu().gpr[2] == 0 &&
          f.rt->cpu().pc == 0x08001800u && f.threads->snapshot()[0].priority == 119,
          "Lowering current priority did not resume caller");
    change(f, 0, 0); check(f.rt->cpu().gpr[2] == 0, "Current priority zero inheritance");
    change(f, low, 0);
    check(f.rt->stopped() && f.rt->cpu().gpr[2] == 0xDEADBEEFu &&
          f.threads->snapshot()[0].priority == 119, "Unsupported tie mutated priority");
    Scheduled same;
    change(same, 0, 8);
    same.rt->invoke_import("ThreadManForUser", 0x94AA61EEu, same.rt->cpu());
    check(same.rt->cpu().gpr[2] == 8, "Priority query ignored mutation");
}
void vfpu_attributes() {
    auto change = [](auto& f, std::uint32_t remove, std::uint32_t add) {
        auto& c = f.rt->cpu(); c.gpr[4] = remove; c.gpr[5] = add;
        c.gpr[2] = 0xDEADBEEFu; c.gpr[31] = 0x08001800u; c.pc = 0x08001900u;
        invoke_returning_import(*f.rt, c, "ThreadManForUser", 0xEA748E31u);
    };
    Scheduled f; const auto child = f.create(8); f.start(child);
    auto& c = f.rt->cpu();
    for (unsigned i = 0; i < c.vfpu.size(); ++i) c.vfpu[i] = static_cast<float>(i) + 0.5f;
    for (unsigned i = 0; i < c.vfpu_ctrl.size(); ++i) c.vfpu_ctrl[i] = i + 1;
    const auto lanes = c.vfpu; const auto controls = c.vfpu_ctrl;
    for (auto masks : {std::pair{0u, 0x4000u}, {0u, 0x4000u}, {0u, 0u}, {0x4000u, 0u}, {0x4000u, 0u}}) {
        const auto previous = f.threads->snapshot()[0].attributes;
        change(f, masks.first, masks.second);
        check(c.gpr[2] == 0 && c.pc == c.gpr[31] &&
              f.threads->snapshot()[0].attributes == ((previous & ~masks.first) | masks.second),
              "VFPU attribute transition/return");
        check(c.vfpu == lanes && c.vfpu_ctrl == controls, "Attribute change reset VFPU state");
    }
    for (unsigned bit = 0; bit < 32; ++bit) {
        if (bit == 14) continue;
        for (bool remove : {false, true}) {
            change(f, remove ? 1u << bit : 0, remove ? 0 : 1u << bit);
            check(c.gpr[2] == 0x80020191u && f.threads->snapshot()[0].attributes == 0x800000FFu,
                  "Invalid attribute bit changed ownership");
        }
    }
    change(f, 0, 0x4000u);
    const auto before = c.gpr;
    f.rt->invoke_import("ThreadManForUser", 0xEA748E31u, c);
    check(c.gpr == before && c.vfpu == lanes && c.vfpu_ctrl == controls, "Attribute ABI clobber");
    change(f, 0x4000u, 0x4000u);
    check(f.rt->stopped() && c.gpr[2] == 0xDEADBEEFu && c.pc == 0x08001900u &&
          f.threads->snapshot()[0].attributes == 0x800040FFu, "Unverified overlap changed attributes");
    Fixture missing; change(missing, 0, 0x4000u);
    check(missing.rt->stopped() && missing.rt->cpu().gpr[2] == 0xDEADBEEFu, "Missing current thread accepted");
    Scheduled suspended; const auto uid = suspended.create(8); suspended.start(uid);
    suspended.rt->invoke_import("Kernel_Library", 0x092968F4u, suspended.rt->cpu());
    change(suspended, 0, 0x4000u);
    check(suspended.rt->stopped() && suspended.threads->snapshot()[0].attributes == 0x800000FFu,
          "Suspended attribute mutation");
    Scheduled isolated; const auto other = isolated.create(8); isolated.start(other);
    check(isolated.threads->snapshot()[0].attributes == 0x800000FFu, "Attribute state leaked");
}
void callback_lifecycle() {
    auto call = [](Scheduled& f, const char* library, std::uint32_t nid,
                   std::uint32_t a0, std::uint32_t a1 = 0, std::uint32_t a2 = 0) {
        auto& c = f.rt->cpu(); c.gpr[4] = a0; c.gpr[5] = a1; c.gpr[6] = a2;
        c.gpr[2] = 0xDEADBEEFu; c.gpr[31] = 0x08001800u; c.pc = 0x08001900u;
        invoke_returning_import(*f.rt, c, library, nid); return c.gpr[2];
    };
    auto create = [&](Scheduled& f, std::uint32_t name = 0x08004000u,
                       std::uint32_t function = arena.executable_begin, std::uint32_t common = 0) {
        return call(f, "ThreadManForUser", 0xE81CAF8Fu, name, function, common);
    };
    auto select = [&](Scheduled& f, std::uint32_t uid) { return call(f, "LoadExecForUser", 0x4AC57943u, uid); };
    auto erase = [&](Scheduled& f, std::uint32_t uid) { return call(f, "ThreadManForUser", 0xEDBA5844u, uid); };
    Scheduled f;
    check(create(f, 0) == 0x80020001u && f.threads->callbacks().empty(), "Null callback name");
    const auto first = create(f, 0x08004000u, 0, 0xDEADBEEFu);
    check(first == 0x50000000u && f.threads->callbacks().at(first).owner == kBootstrapUid &&
          f.threads->callbacks().at(first).function == 0 &&
          f.threads->callbacks().at(first).common == 0xDEADBEEFu, "Callback metadata ownership");
    for (unsigned i = 0; i < 40; ++i) f.rt->memory().store8(0x08004000u + i, 'c');
    const auto second = create(f); const auto third = create(f);
    f.rt->memory().store8(0x08004000u, 'x');
    check(third > second && f.threads->callbacks().at(second).name == std::string(31, 'c'), "Copied callback name/duplicates");
    check(select(f, first) == 0 && select(f, second) == 0 && f.threads->exit_callback() == second,
          "Exit callback replacement");
    for (auto uid : {0u, 1u, 0xFFFFFFFFu})
        check(select(f, uid) == 0x800200D2u && f.threads->exit_callback() == second, "Invalid exit callback changed registration");
    const auto child = f.create(8); f.start(child);
    check(erase(f, second) == 0 && f.threads->exit_callback() == 0 &&
          erase(f, second) == 0x800201A1u && erase(f, 0) == 0x800201A1u, "Cross-thread callback deletion/stale UID");
    const auto owned = create(f); select(f, owned);
    check(f.threads->callbacks().at(owned).owner == child, "Child callback owner");
    f.rt->invoke_import("ThreadManForUser", 0x809CE29Bu, f.rt->cpu());
    check(f.threads->current_uid() == kBootstrapUid && !f.threads->callbacks().contains(owned) &&
          f.threads->exit_callback() == 0 && f.threads->callbacks().contains(first), "Thread deletion callback cleanup");
    const auto count = f.threads->callbacks().size(); f.exit();
    check(f.rt->stopped() && f.threads->current_uid() == kBootstrapUid &&
          f.threads->callbacks().size() == count, "Unverified owner-exit lifetime mutated state");
    for (auto function : {arena.executable_begin + 1, arena.executable_end, 0xFFFFFFFFu}) {
        Scheduled bad; check(create(bad, 0x08004000u, function) == 0xDEADBEEFu && bad.rt->stopped() &&
                             bad.threads->callbacks().empty(), "Unsupported callback entry accepted");
    }
    for (auto address : {0xFFFFFFFFu, 0x88004000u, 0x09FFFFFFu}) {
        Scheduled bad; bad.rt->memory().store8(0x09FFFFFFu, 'c');
        check(create(bad, address) == 0xDEADBEEFu && bad.rt->stopped() && bad.threads->callbacks().empty(),
              "Invalid callback name mutated registry");
    }
    Scheduled suspended; suspended.rt->invoke_import("Kernel_Library", 0x092968F4u, suspended.rt->cpu());
    check(create(suspended) == 0xDEADBEEFu && suspended.rt->stopped() && suspended.threads->callbacks().empty(),
          "Suspended callback creation");
    Scheduled capacity;
    for (unsigned i = 0; i < 4096; ++i) check(create(capacity) == first + i, "Callback capacity/isolated UIDs");
    check(create(capacity) == 0x80020190u && erase(capacity, first) == 0 &&
          create(capacity) == first + 4096, "Callback exhaustion consumed or reused UID");
}
void inactive_timers() {
    Scheduled f;
    auto call = [](Scheduled& fixture, std::uint32_t nid, std::uint32_t arg, std::uint32_t option = 0) {
        auto& c = fixture.rt->cpu(); c.gpr[4] = arg; c.gpr[5] = option;
        c.gpr[2] = 0xDEADBEEFu; c.gpr[31] = 0x08001800u; c.pc = 0x08001900u;
        invoke_returning_import(*fixture.rt, c, "ThreadManForUser", nid); return c.gpr[2];
    };
    check(call(f, 0x20FFF560u, 0) == 0x80020001u && f.threads->timers().empty(), "Null timer name");
    const auto first = call(f, 0x20FFF560u, 0x08004000u);
    check(first == 0x40000000u && f.threads->timers().at(first).name.empty(), "Empty timer name/UID");
    for (unsigned i = 0; i < 40; ++i) f.rt->memory().store8(0x08004000u + i, 'v');
    const auto second = call(f, 0x20FFF560u, 0x08004000u);
    check(f.threads->timers().at(second).name.size() == 31, "Timer name truncation");
    check(call(f, 0x20FFF560u, 0x08004000u) > second, "Duplicate timer names/UIDs");
    check(call(f, 0x328F9E52u, first) == 0 && call(f, 0x328F9E52u, first) == 0x800201BEu, "Timer delete/stale UID");
    check(call(f, 0xC68D9437u, first) == 0x800201BEu && call(f, 0xC68D9437u, 0) == 0x800201BFu,
          "Invalid timer start IDs");
    const auto count = f.threads->timers().size();
    check(call(f, 0xC68D9437u, second) == 0xDEADBEEFu && f.rt->stopped() &&
          f.rt->cpu().pc == 0x08001900u && f.threads->timers().size() == count, "Timer start fabricated clock progress");
    for (auto address : {0xFFFFFFFFu, 0x88004000u, 0x09FFFFFFu}) {
        Scheduled bad; bad.rt->memory().store8(0x09FFFFFFu, 'v');
        check(call(bad, 0x20FFF560u, address) == 0xDEADBEEFu && bad.rt->stopped() &&
              bad.threads->timers().empty(), "Invalid timer name changed registry");
    }
    Scheduled options;
    check(call(options, 0x20FFF560u, 0x08004000u, 0x08004100u) == 0xDEADBEEFu &&
          options.rt->stopped() && options.threads->timers().empty(), "Unsupported timer options accepted");
    Scheduled isolated;
    check(call(isolated, 0x20FFF560u, 0x08004000u) == first, "Timer registry leaked across runtimes");
    for (unsigned i = 1; i < 4096; ++i)
        check(call(isolated, 0x20FFF560u, 0x08004000u) == first + i, "Timer UID allocation");
    check(call(isolated, 0x20FFF560u, 0x08004000u) == 0x80020190u, "Timer metadata limit");
    check(call(isolated, 0x328F9E52u, first) == 0 &&
          call(isolated, 0x20FFF560u, 0x08004000u) == first + 4096, "Timer exhaustion consumed/reused UID");
    Scheduled suspended;
    suspended.rt->invoke_import("Kernel_Library", 0x092968F4u, suspended.rt->cpu());
    check(call(suspended, 0x20FFF560u, 0x08004000u) == 0xDEADBEEFu && suspended.rt->stopped() &&
          suspended.threads->timers().empty(), "Timer mutation while interrupts suspended");
}
void mutex_boundaries() {
    Scheduled wait;
    const auto uid = mutex_call(wait, 0xB7D098C6u, 0x08004000u, 0, 1);
    const auto high = wait.create(8); wait.start(high);
    mutex_call(wait, 0xB011B11Fu, uid, 1);
    check(wait.rt->stopped() && wait.threads->mutexes().at(uid).owner == kBootstrapUid &&
          wait.threads->mutexes().at(uid).count == 1, "Unimplemented wait fabricated success");
    Scheduled timed; const auto t = mutex_call(timed, 0xB7D098C6u, 0x08004000u);
    mutex_call(timed, 0xB011B11Fu, t, 1, 0x08004100u);
    check(timed.rt->stopped() && timed.threads->mutexes().at(t).count == 0, "Unsupported timeout acquired lock");
    Scheduled bad; mutex_call(bad, 0xB7D098C6u, 0xFFFFFFFFu);
    check(bad.rt->stopped() && bad.threads->mutexes().empty(), "Invalid name mutated mutex registry");
}
}
int main() {
    try { ownership(); failures(); names_and_stops(); arena_validation(); scheduling(); start_arguments(); interrupt_state();
          mutex_ownership(); mutex_boundaries(); current_priority(); priority_changes(); vfpu_attributes(); callback_lifecycle(); inactive_timers();
          std::cout << "Dormant thread ownership and failure contracts passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
