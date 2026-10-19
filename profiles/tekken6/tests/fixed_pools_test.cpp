#include "azazel6/threads.hpp"
#include "azazel6/bootstrap.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <tuple>

using namespace azazel6;
namespace {
constexpr std::uint32_t begin = 0x08010000u, end = 0x08020000u;
constexpr std::uint32_t name = 0x08003000u, options = 0x08003100u, output = 0x08004000u;
constexpr std::uint32_t create_nid = 0xC07BB470u, delete_nid = 0xED1410E0u;
constexpr std::uint32_t try_nid = 0x623AE665u, allocate_nid = 0xD979E9BFu, free_nid = 0xF6414A71u;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Fixture {
    std::unique_ptr<psprecomp::Runtime> rt = std::make_unique<psprecomp::Runtime>();
    std::shared_ptr<GuestAllocator> allocator = std::make_shared<GuestAllocator>(begin, end);
    std::shared_ptr<ThreadRegistry> threads;
    Fixture() {
        threads = register_thread_hle(*rt, {begin, end, 0x08001000u, begin,
            0x08001000u, 0x08002000u, name, 32, 512}, allocator);
        rt->memory().store8(name, 'p'); rt->memory().store8(name + 1, 0);
        rt->memory().store32(output, 0x11223344u);
        rt->memory().store32(options, 8); rt->memory().store32(options + 4, 16);
        args();
    }
    void args() {
        auto& c = rt->cpu(); c.gpr[4] = name; c.gpr[5] = 2; c.gpr[6] = 0x100;
        c.gpr[7] = 513; c.gpr[8] = 1; c.gpr[9] = options;
    }
    std::uint32_t call(std::uint32_t nid) {
        auto& c = rt->cpu(); c.gpr[2] = 0xDEADBEEFu; c.pc = 0x08001900u; c.gpr[31] = 0x08001800u;
        invoke_returning_import(*rt, c, "ThreadManForUser", nid); return c.gpr[2];
    }
    std::uint32_t allocate(std::uint32_t uid, std::uint32_t nid = try_nid) {
        auto& c = rt->cpu(); c.gpr[4] = uid; c.gpr[5] = output; c.gpr[6] = 0; return call(nid);
    }
};
void lifecycle() {
    Fixture f; const auto before = f.rt->cpu().gpr;
    const auto uid = f.call(create_nid);
    check(uid == 0x60000000u && f.rt->cpu().pc == 0x08001800u, "Create UID/return ABI");
    for (std::size_t i = 3; i < 31; ++i) check(f.rt->cpu().gpr[i] == before[i], "Create clobber");
    auto pool = f.threads->fixed_pools().at(uid);
    check(pool.name == "p" && pool.block_size == 513 && pool.alignment == 16 &&
        pool.attributes == 0x100 && !pool.allocated && pool.allocation.base == begin &&
        f.allocator->total_free() == end - begin - 768, "Pool ownership");
    f.rt->memory().store32(pool.allocation.base, 0xCAFEBABEu);
    check(f.allocate(uid) == 0 && f.rt->memory().load32(output) == pool.allocation.base &&
        f.rt->memory().load32(pool.allocation.base) == 0xCAFEBABEu, "Allocation output/data");
    f.rt->memory().store32(output, 0x11223344u);
    check(f.allocate(uid) == 0x80020190u && f.rt->memory().load32(output) == 0x11223344u,
        "Exhausted try changed output");
    for (auto pointer : {0u, begin + 1, begin + 512, end}) {
        f.rt->cpu().gpr[4] = uid; f.rt->cpu().gpr[5] = pointer;
        check(f.call(free_nid) == 0x800201B6u && f.threads->fixed_pools().at(uid).allocated,
            "Free accepted interior/foreign/null address");
    }
    f.rt->cpu().gpr[5] = begin;
    check(f.call(free_nid) == 0 && f.call(free_nid) == 0x800201B6u, "Free/double free");
    check(f.allocate(uid, allocate_nid) == 0, "Immediate blocking API allocation");
    f.rt->cpu().gpr[4] = uid; check(f.call(delete_nid) == 0, "Delete live allocation");
    check(f.allocator->total_free() == end - begin && f.threads->fixed_pools().empty(), "Pool release");
    for (auto nid : {try_nid, allocate_nid, free_nid, delete_nid}) {
        f.rt->cpu().gpr[4] = uid; check(f.call(nid) == 0x8002019Du, "Stale UID");
        f.rt->cpu().gpr[4] = 0x10000000u; check(f.call(nid) == 0x8002019Du, "Cross-type UID");
    }
    f.args(); const auto newer = f.call(create_nid); check(newer == uid + 1, "UID reused");
    Fixture isolated; check(isolated.allocate(newer) == 0x8002019Du, "Cross-runtime pool leak");
}
void allocation_bounds() {
    Fixture f;
    auto reservation = f.allocator->allocate(0xFE00);
    check(reservation && f.call(create_nid) == 0x80020190u && f.threads->fixed_pools().empty(), "Shared exhaustion");
    f.allocator->release(reservation->ticket);
    f.rt->cpu().gpr[6] = 0x4100; f.rt->memory().store32(options + 4, 4096);
    const auto uid = f.call(create_nid);
    check(f.threads->fixed_pools().at(uid).allocation.base == 0x0801F000u, "High aligned pool");
    f.args(); const auto second = f.call(create_nid);
    check(second != uid && f.threads->fixed_pools().at(second).allocation.base == begin, "Duplicate name/separate ownership");
    f.rt->cpu().gpr[4] = uid; f.call(delete_nid);
    check(f.threads->fixed_pools().contains(second), "Delete released unrelated pool");
    for (std::uint32_t i = 0; i < 31; ++i) f.rt->memory().store8(name + i, 'n');
    f.args(); f.rt->cpu().gpr[9] = 0; const auto long_name = f.call(create_nid);
    check(f.threads->fixed_pools().at(long_name).name.size() == 31, "Bounded copied name");
    f.rt->memory().store8(name, 0); check(f.threads->fixed_pools().at(long_name).name[0] == 'n', "Name borrowed");
    for (const auto alignment : {0u, 1u, 2u, 4u, 8u, 16u, 256u, 4096u}) {
        Fixture a; a.rt->memory().store32(options + 4, alignment);
        const auto id = a.call(create_nid);
        check(a.threads->fixed_pools().at(id).allocation.base % std::max(alignment, 8u) == 0, "Alignment contract");
    }
}
void create_rejections() {
    for (auto [reg, value, expected] : {std::tuple{4, 0u, 0x80020190u}, {6, 0x200u, 0x80020191u},
        {7, 0u, 0x800201B7u}, {7, 0xFFFFFFFFu, 0x800201B7u}, {8, 0u, 0x800201B7u},
        {8, 0xFFFFFFFFu, 0x800201B7u}, {7, 0x7FFFFFFFu, 0x80020190u}}) {
        Fixture f; f.rt->cpu().gpr[reg] = value;
        check(f.call(create_nid) == expected && f.threads->fixed_pools().empty() &&
            f.allocator->total_free() == end - begin, "Create error mutated ownership");
    }
    for (auto [reg, value] : {std::pair{5, 6u}, {6, 1u}, {8, 2u}, {9, options + 1},
        {9, 0xFFFFFFFCu}, {4, 0xFFFFFFFFu}, {4, 0x88003000u}}) {
        Fixture f; f.rt->cpu().gpr[reg] = value;
        check(f.call(create_nid) == 0xDEADBEEFu && f.rt->stopped() &&
            f.threads->fixed_pools().empty() && f.allocator->total_free() == end - begin,
            "Unsupported create did not fail closed");
    }
    for (auto alignment : {3u, 36u, 0xFFFFFFFFu}) {
        Fixture f; f.rt->memory().store32(options + 4, alignment);
        check(f.call(create_nid) == 0x800200D2u && f.threads->fixed_pools().empty(), "Invalid alignment accepted");
    }
    for (auto option_size : {0u, 7u, 16u, 0xFFFFFFFFu}) {
        Fixture f; f.rt->memory().store32(options, option_size); f.call(create_nid);
        check(f.rt->stopped() && f.threads->fixed_pools().empty(), "Unknown option layout accepted");
    }
    Fixture truncated;
    const auto last = 0x08000000u + truncated.rt->memory().size() - 4;
    truncated.rt->memory().store32(last, 8); truncated.rt->cpu().gpr[9] = last;
    truncated.call(create_nid); check(truncated.rt->stopped(), "Truncated option read");
}
void mutation_rejections() {
    for (auto address : {0u, output + 1, 0x08001000u, 0x88004000u, 0xFFFFFFFCu}) {
        Fixture f; const auto uid = f.call(create_nid);
        f.rt->cpu().gpr[4] = uid; f.rt->cpu().gpr[5] = address;
        check(f.call(try_nid) == 0xDEADBEEFu && f.rt->stopped() &&
            !f.threads->fixed_pools().at(uid).allocated && f.rt->cpu().pc == 0x08001900u,
            "Invalid output allocated a block or returned");
    }
    Fixture timeout; const auto timed = timeout.call(create_nid);
    timeout.rt->cpu().gpr[4] = timed; timeout.rt->cpu().gpr[5] = output; timeout.rt->cpu().gpr[6] = options;
    timeout.call(allocate_nid);
    check(timeout.rt->stopped() && !timeout.threads->fixed_pools().at(timed).allocated, "Timeout allocated block");
    Fixture wait; const auto full = wait.call(create_nid); wait.allocate(full);
    check(wait.allocate(full, allocate_nid) == 0xDEADBEEFu && wait.rt->stopped() &&
        wait.threads->fixed_pools().at(full).allocated, "Contended allocation did not stop");
    for (auto nid : {create_nid, delete_nid, try_nid, allocate_nid, free_nid}) {
        Fixture f; const auto uid = f.call(create_nid);
        f.rt->invoke_import("Kernel_Library", 0x092968F4u, f.rt->cpu());
        if (nid == create_nid) f.args(); else f.rt->cpu().gpr[4] = uid;
        check(f.call(nid) == 0xDEADBEEFu && f.rt->stopped() && f.threads->fixed_pools().size() == 1,
            "Disabled interrupt context changed pools");
    }
}
}
int main() {
    try { lifecycle(); allocation_bounds(); create_rejections(); mutation_rejections();
        std::cout << "Single-block fixed pool contracts passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
