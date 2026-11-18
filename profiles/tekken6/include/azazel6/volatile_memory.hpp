#pragma once
#include "azazel6/memory.hpp"
#include <cstdint>
#include <memory>

namespace azazel6 {
inline constexpr std::uint32_t kVolatileMemoryBase = 0x08400000u;
inline constexpr std::uint32_t kVolatileMemorySize = 0x00400000u;
inline constexpr std::uint32_t kVolatileMemTryLockNid = 0xA14F40B2u;
inline constexpr std::uint32_t kVolatileMemUnlockNid = 0xA569E425u;

struct VolatileMemoryState {
    const psprecomp::Runtime* owner = nullptr;
    bool locked = false;
    std::uint32_t successful_locks = 0;
};

// Only the observed nonblocking mode-0 lock and its release are supported.
// Output words must belong to live user partition blocks; no host pointer is
// ever exposed to the guest.
std::shared_ptr<VolatileMemoryState> register_volatile_memory_hle(
    psprecomp::Runtime&, std::shared_ptr<const PartitionMemory>);
} // namespace azazel6
