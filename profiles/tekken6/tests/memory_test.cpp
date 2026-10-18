#include "azazel6/memory.hpp"
#include "azazel6/threads.hpp"
#include "azazel6/bootstrap.hpp"
#include <iostream>
#include <stdexcept>
using namespace azazel6;
void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void allocator_contracts() {
    GuestAllocator a(0x08010000u, 0x08020000u);
    const auto low = a.allocate(1), high = a.allocate(512, 4096, true);
    check(low && high && low->base == 0x08010000u && high->base == 0x0801F000u, "Aligned low/high allocation");
    check(a.total_free() == 65536 - 768 && a.max_free() == 0xEF00u, "Free size accounting");
    check(!a.allocate(0) && !a.allocate(0xFFFFFFFFu) && !a.allocate(256, 3), "Overflow/alignment accepted");
    check(!a.allocate(256, 256, false, low->base), "Overlap accepted");
    check(!a.allocate(256, 256, false, 0x0800FF00u), "Outside fixed allocation accepted");
    const auto fixed = a.allocate(256, 256, false, 0x08010800u);
    check(fixed && a.release(fixed->ticket) && !a.release(fixed->ticket), "Ticket lifetime");
    check(a.release(low->ticket) && a.release(high->ticket) && a.max_free() == 65536, "Free coalescing");
}
void hle_contracts() {
    auto rt = std::make_unique<psprecomp::Runtime>();
    auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
    auto memory = register_memory_hle(*rt, allocator);
    auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
                                           0x08001000u, 0x08002000u, 0x08003000u}, allocator);
    auto& c = rt->cpu(); rt->memory().store8(0x08004000u, 0);
    auto allocate = [&](std::uint32_t type, std::uint32_t size, std::uint32_t address) {
        c.gpr[4] = 2; c.gpr[5] = 0x08004000u; c.gpr[6] = type; c.gpr[7] = size; c.gpr[8] = address;
        rt->invoke_import("SysMemUserForUser", 0x237DBD4Fu, c); return c.gpr[2];
    };
    const auto block = allocate(3, 0xF000u, 4096);
    check(block >= 0x10000000u && block < 0x80000000u, "User aligned allocation");
    c.gpr[4] = block; rt->invoke_import("SysMemUserForUser", 0x9D9A5BA1u, c);
    check(c.gpr[2] == 0x08010000u, "Block head");
    c.gpr[4] = 0x08004000u; c.gpr[5] = 0x08001000u; c.gpr[6] = 40; c.gpr[7] = 4096; c.gpr[8] = 0; c.gpr[9] = 0;
    rt->invoke_import("ThreadManForUser", kCreateThreadNid, c);
    const auto thread = c.gpr[2];
    check(thread == 1 && threads->snapshot()[0].stack_base == 0x0801F000u, "Thread/heap overlap");
    check(allocate(0, 1, 0) == 0x800200D9u && allocator->total_free() == 0, "Shared exhaustion");
    c.gpr[4] = thread; rt->invoke_import("SysMemUserForUser", 0xB6D61D02u, c);
    check(c.gpr[2] == 0x800200CBu && allocator->total_free() == 0, "Cross-type UID freed stack");
    c.gpr[4] = thread; rt->invoke_import("ThreadManForUser", kDeleteThreadNid, c);
    check(allocator->total_free() == 4096, "Stack release not shared");
    c.gpr[4] = block; rt->invoke_import("SysMemUserForUser", 0xB6D61D02u, c);
    check(c.gpr[2] == 0 && allocator->max_free() == 65536, "Block release");
    rt->invoke_import("SysMemUserForUser", 0x9D9A5BA1u, c); check(c.gpr[2] == 0, "Stale block head");
    check(allocate(3, 256, 0) == 0x800200E4u && allocate(3, 256, 3) == 0x800200E4u, "Invalid alignment code");
    check(allocate(5, 256, 0) == 0x800200D8u && allocate(0, 0xFFFFFFFFu, 0) == 0x800200D9u, "Type/size rejection");
    const auto fixed = allocate(2, 1, 0x080108FFu);
    check(memory->blocks.at(fixed).base == 0x08010800u && memory->blocks.at(fixed).size == 256, "Fixed byte-range rounding");
    c.gpr[4] = 2; c.gpr[5] = 0xFFFFFFFFu; c.gpr[6] = 0; c.gpr[7] = 256;
    rt->invoke_import("SysMemUserForUser", 0x237DBD4Fu, c); check(c.gpr[2] == 0x800200D3u, "Name pointer check");
}
void cache_contracts() {
    constexpr std::uint32_t begin = 0x08010000u, end = 0x08030000u;
    auto rt = std::make_unique<psprecomp::Runtime>();
    const auto state = register_data_cache_hle(*rt, begin, end);
    auto call = [](auto& runtime, std::uint32_t address, std::uint32_t size) {
        auto& c = runtime.cpu(); c.gpr[4] = address; c.gpr[5] = size;
        c.gpr[2] = 0xDEADBEEFu; c.gpr[31] = 0x08001800u; c.pc = 0x08001900u;
        invoke_returning_import(runtime, c, "UtilsForUser", 0x34B9FA9Eu);
    };
    auto view = rt->memory().aot_fast_view(); view.aot_store32(begin, 0x12345678u);
    call(*rt, begin + 1, 3);
    check(state->completed_ranges == 1 && state->last_address == begin + 1 && state->last_size == 3 &&
          rt->cpu().gpr[2] == 0 && rt->cpu().pc == 0x08001800u &&
          rt->memory().load32(begin) == 0x12345678u, "Cache range lost coherent AOT data");
    const auto registers = rt->cpu().gpr;
    rt->invoke_import("UtilsForUser", 0x34B9FA9Eu, rt->cpu());
    check(registers == rt->cpu().gpr, "Cache ABI clobber");
    call(*rt, end, 0); call(*rt, end - 1, 1); call(*rt, begin + 1, 65535);
    check(!rt->stopped() && state->completed_ranges == 5, "Cache range boundary");
    for (auto [address, size] : {std::pair{begin - 1, 1u}, {end, 1u}, {0xFFFFFFFFu, 2u},
         {begin, 0xFFFFFFFFu}, {begin, 65536u}, {0x88010000u, 4u}, {0u, 0u}}) {
        auto bad = std::make_unique<psprecomp::Runtime>();
        const auto rejected = register_data_cache_hle(*bad, begin, end);
        call(*bad, address, size);
        check(bad->stopped() && bad->cpu().gpr[2] == 0xDEADBEEFu && bad->cpu().pc == 0x08001900u &&
              !rejected->completed_ranges, "Unsupported cache range accepted");
    }
    rt->register_function(begin + 4, [](auto&, auto&) {}, "synthetic_code");
    call(*rt, begin + 63, 1);
    check(rt->stopped() && state->completed_ranges == 5, "Cache-line code overlap accepted");
    bool rejected = false;
    try { register_data_cache_hle(*rt, begin + 1, end); }
    catch (const std::exception&) { rejected = true; }
    check(rejected, "Unaligned cache ownership accepted");
}
int main() {
    try { allocator_contracts(); hle_contracts(); cache_contracts(); std::cout << "Shared guest allocation contracts passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
