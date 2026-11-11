#pragma once
#include "psprecomp/runtime.hpp"
#include <cstdint>
#include <memory>

namespace azazel6 {
inline constexpr std::uint32_t kCtrlSetIdleCancelThresholdNid = 0xA7144800u;
inline constexpr std::uint32_t kCtrlSetSamplingCycleNid = 0x6A2774F3u;
inline constexpr std::uint32_t kCtrlSetSamplingModeNid = 0x1F4011E6u;

struct ControllerSettings {
    std::int32_t idle_reset = -1;
    std::int32_t idle_back = -1;
    std::uint32_t writes{};
    std::uint32_t sampling_mode{};
    std::uint32_t cycle_writes{}, mode_writes{};
};

[[nodiscard]] std::shared_ptr<ControllerSettings> register_controller_settings_hle(psprecomp::Runtime&);
} // namespace azazel6
