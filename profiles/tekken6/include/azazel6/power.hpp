#pragma once
#include "psprecomp/runtime.hpp"
#include <cstdint>
#include <memory>

namespace azazel6 {
inline constexpr std::uint32_t kPowerGetPllClockFrequencyIntNid = 0x34F9C463u;
inline constexpr std::uint32_t kPowerGetCpuClockFrequencyIntNid = 0xFDB5BFE9u;
inline constexpr std::uint32_t kPowerGetBusClockFrequencyIntNid = 0xBD681969u;

// Nominal PSP startup clocks in MHz. This state is read-only until an observed
// path requires and verifies a clock transition contract.
struct PowerClocks {
    std::uint32_t pll_mhz{222};
    std::uint32_t cpu_mhz{222};
    std::uint32_t bus_mhz{111};
    std::uint32_t pll_reads{};
    std::uint32_t cpu_reads{};
    std::uint32_t bus_reads{};
};

[[nodiscard]] std::shared_ptr<PowerClocks> register_power_clock_hle(psprecomp::Runtime&);
} // namespace azazel6
