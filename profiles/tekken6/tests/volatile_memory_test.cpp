#include "azazel6/volatile_memory.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

using namespace azazel6;
namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

struct Fixture {
    std::unique_ptr<psprecomp::Runtime> rt = std::make_unique<psprecomp::Runtime>();
    std::shared_ptr<PartitionMemory> partition = std::make_shared<PartitionMemory>();
    std::shared_ptr<VolatileMemoryState> state;
    GuestAllocation block{};

    Fixture() {
        partition->owner = rt.get();
        partition->allocator = std::make_shared<GuestAllocator>(0x09000000u, 0x09001000u);
        const auto allocation = partition->allocator->allocate(256u);
        check(allocation.has_value(), "Volatile fixture allocation");
        block = *allocation;
        partition->blocks.emplace(1u, block);
        state = register_volatile_memory_hle(*rt, partition);
    }
    std::uint32_t call(std::uint32_t nid, std::uint32_t mode = 0u,
                       std::uint32_t address = 0u, std::uint32_t size = 0u) {
        auto& c = rt->cpu();
        c.gpr[2] = 0xDEADBEEFu;
        c.gpr[4] = mode; c.gpr[5] = address; c.gpr[6] = size;
        rt->invoke_import("sceSuspendForUser", nid, c);
        return c.gpr[2];
    }
};

void mode_and_lifetime() {
    Fixture f;
    constexpr auto address = 0x09000000u;
    auto& memory = f.rt->memory();
    memory.store32(address + 8u, 0x11111111u);
    memory.store32(address + 12u, 0x22222222u);
    check(f.call(kVolatileMemTryLockNid, 1u, address, address + 4u) == 0x80000107u &&
          !f.state->locked && memory.load32(address) == 0u, "Invalid mode changed lock or output");
    check(f.call(kVolatileMemUnlockNid) == 0x800201AEu, "Unlocked release code");
    check(f.call(kVolatileMemTryLockNid, 0u, address, address + 4u) == 0u &&
          memory.load32(address) == kVolatileMemoryBase &&
          memory.load32(address + 4u) == kVolatileMemorySize &&
          f.state->locked && f.state->successful_locks == 1u, "Mode-0 lock outputs");
    check(f.call(kVolatileMemTryLockNid, 0u, address + 8u, address + 12u) == 0x802B0200u &&
          memory.load32(address + 8u) == 0x11111111u &&
          memory.load32(address + 12u) == 0x22222222u, "Busy lock changed outputs");
    check(f.call(kVolatileMemUnlockNid, 1u) == 0x80000107u && f.state->locked,
          "Invalid release changed ownership");
    check(f.call(kVolatileMemUnlockNid) == 0u && !f.state->locked, "Release failed");
    check(f.call(kVolatileMemTryLockNid) == 0u && f.state->successful_locks == 2u,
          "Null outputs or reacquisition failed");
    check(f.call(kVolatileMemUnlockNid) == 0u, "Second release failed");
    check(f.call(kVolatileMemTryLockNid, 0u, address, 0u) == 0u &&
          memory.load32(address) == kVolatileMemoryBase, "Single optional output failed");
}

void output_boundaries() {
    for (auto [address, size] : {
             std::pair{0x09000001u, 0x09000004u},
             std::pair{0x090000FCu, 0x09000100u},
             std::pair{0x09000000u, 0x09000000u},
             std::pair{0xFFFFFFFFu, 0x09000004u},
             std::pair{0x08001000u, 0x09000004u},
             std::pair{0x09000000u, 0x08001000u}}) {
        Fixture f;
        f.rt->memory().store32(0x09000000u, 0x12345678u);
        check(f.call(kVolatileMemTryLockNid, 0u, address, size) == 0xDEADBEEFu &&
              f.rt->stopped() && !f.state->locked && f.state->successful_locks == 0u &&
              f.rt->memory().load32(0x09000000u) == 0x12345678u,
              "Invalid output accepted or partially written");
    }
    Fixture code;
    code.rt->register_function(0x09000000u, [](auto&, auto&) {}, "synthetic_code");
    check(code.call(kVolatileMemTryLockNid, 0u, 0x09000000u, 0u) == 0xDEADBEEFu &&
          code.rt->stopped(), "Code output accepted");
    Fixture stale;
    stale.partition->blocks.clear();
    check(stale.call(kVolatileMemTryLockNid, 0u, 0x09000000u, 0u) == 0xDEADBEEFu &&
          stale.rt->stopped(), "Released partition output accepted");
    Fixture foreign;
    auto other = std::make_unique<psprecomp::Runtime>();
    bool rejected = false;
    try { (void)register_volatile_memory_hle(*other, foreign.partition); }
    catch (const std::exception&) { rejected = true; }
    check(rejected, "Foreign partition ownership accepted");
}
} // namespace

int main() {
    try {
        mode_and_lifetime();
        output_boundaries();
        std::cout << "Volatile memory contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
