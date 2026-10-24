#pragma once
#include "psprecomp/runtime.hpp"
#include <cstdint>

namespace azazel6 {
// Shared by the primary HLE and optional upstream comparison. The caller must
// establish a running, non-interrupt thread and pass its live stack lease.
inline void convert_sysclock_wide(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c,
                                 std::uint32_t stack_base, std::uint32_t stack_size) {
    const auto seconds = c.gpr[6], microseconds = c.gpr[7];
    if ((!seconds && !microseconds) || (seconds && seconds == microseconds)) {
        rt.stop("Unsupported clock-conversion output alias"); return;
    }
    for (const auto address : {seconds, microseconds}) {
        if (address && ((address & 3u) || address < stack_base ||
            static_cast<std::uint64_t>(address) + 4 > static_cast<std::uint64_t>(stack_base) + stack_size ||
            address < psprecomp::GuestMemory::kPhysicalBase ||
            static_cast<std::uint64_t>(address) + 4 >
                static_cast<std::uint64_t>(psprecomp::GuestMemory::kPhysicalBase) + rt.memory().size() ||
            rt.has_function(address))) {
            rt.stop("Clock-conversion output outside live caller stack"); return;
        }
    }
    const auto ticks = static_cast<std::uint64_t>(c.gpr[4]) | (static_cast<std::uint64_t>(c.gpr[5]) << 32);
    if (seconds) rt.memory().store32(seconds, static_cast<std::uint32_t>(ticks / 1000000u));
    // Null first output returns the whole low word, not the subsecond remainder.
    if (microseconds) rt.memory().store32(microseconds,
        static_cast<std::uint32_t>(seconds ? ticks % 1000000u : ticks));
    c.set_gpr(2, 0);
}
}
