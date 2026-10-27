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

inline void convert_sysclock(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c,
                             std::uint32_t stack_base, std::uint32_t stack_size) {
    const auto input = c.gpr[4], seconds = c.gpr[5], microseconds = c.gpr[6];
    const auto stack_end = static_cast<std::uint64_t>(stack_base) + stack_size;
    auto stack_word = [&](std::uint32_t address, std::uint32_t size) {
        return address >= stack_base && !(address & 3u) &&
            static_cast<std::uint64_t>(address) + size <= stack_end &&
            address >= psprecomp::GuestMemory::kPhysicalBase &&
            static_cast<std::uint64_t>(address) + size <=
                static_cast<std::uint64_t>(psprecomp::GuestMemory::kPhysicalBase) + rt.memory().size() &&
            !rt.has_function(address);
    };
    if (!stack_word(input, 8) || (!seconds && !microseconds) ||
        (seconds && seconds == microseconds) || (seconds && !stack_word(seconds, 4)) ||
        (microseconds && !stack_word(microseconds, 4))) {
        rt.stop("Clock-conversion range outside live caller stack"); return;
    }
    const auto ticks = static_cast<std::uint64_t>(rt.memory().load32(input)) |
        (static_cast<std::uint64_t>(rt.memory().load32(input + 4)) << 32);
    if (seconds) rt.memory().store32(seconds, static_cast<std::uint32_t>(ticks / 1000000u));
    if (microseconds) rt.memory().store32(microseconds,
        static_cast<std::uint32_t>(seconds ? ticks % 1000000u : ticks));
    c.set_gpr(2, 0);
}
}
