#include "azazel6/volatile_memory.hpp"
#include "psprecomp/common.hpp"

namespace azazel6 {
namespace {
constexpr std::uint32_t invalid_mode = 0x80000107u;
constexpr std::uint32_t in_use = 0x802B0200u;
constexpr std::uint32_t not_locked = 0x800201AEu;

bool owned_output(const psprecomp::Runtime& rt, const PartitionMemory& partition,
                  std::uint32_t address) {
    if (!address) return true;
    if ((address & 3u) || !rt.memory().contains(address, 4) || rt.has_function(address)) return false;
    const auto finish = static_cast<std::uint64_t>(address) + 4u;
    for (const auto& [uid, block] : partition.blocks) {
        (void)uid;
        if (address >= block.base && finish <= static_cast<std::uint64_t>(block.base) + block.size)
            return true;
    }
    return false;
}
} // namespace

std::shared_ptr<VolatileMemoryState> register_volatile_memory_hle(
    psprecomp::Runtime& rt, std::shared_ptr<const PartitionMemory> partition) {
    if (!partition || partition->owner != &rt || !partition->allocator ||
        !rt.memory().contains(kVolatileMemoryBase, kVolatileMemorySize))
        throw psprecomp::Error("Volatile memory requires owned guest RAM and partition state");
    auto state = std::make_shared<VolatileMemoryState>();
    state->owner = &rt;
    rt.register_hle("sceSuspendForUser", kVolatileMemTryLockNid,
        [state, partition](auto& runtime, auto& c) {
            if (c.gpr[4] != 0u) { c.set_gpr(2, invalid_mode); return; }
            if (state->locked) { c.set_gpr(2, in_use); return; }
            const auto address_out = c.gpr[5], size_out = c.gpr[6];
            if (!owned_output(runtime, *partition, address_out) ||
                !owned_output(runtime, *partition, size_out) ||
                (address_out && address_out == size_out)) {
                runtime.stop("Volatile memory output outside distinct live partition words"); return;
            }
            if (address_out) runtime.memory().store32(address_out, kVolatileMemoryBase);
            if (size_out) runtime.memory().store32(size_out, kVolatileMemorySize);
            state->locked = true;
            ++state->successful_locks;
            c.set_gpr(2, 0u);
        });
    rt.register_hle("sceSuspendForUser", kVolatileMemUnlockNid,
        [state](auto&, auto& c) {
            if (c.gpr[4] != 0u) { c.set_gpr(2, invalid_mode); return; }
            if (!state->locked) { c.set_gpr(2, not_locked); return; }
            state->locked = false;
            c.set_gpr(2, 0u);
        });
    return state;
}
} // namespace azazel6
