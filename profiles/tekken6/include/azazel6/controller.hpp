#pragma once
#include "psprecomp/runtime.hpp"
#include "azazel6/input.hpp"
#include <cstdint>
#include <functional>
#include <memory>

namespace azazel6 {
inline constexpr std::uint32_t kCtrlSetIdleCancelThresholdNid = 0xA7144800u;
inline constexpr std::uint32_t kCtrlSetSamplingCycleNid = 0x6A2774F3u;
inline constexpr std::uint32_t kCtrlSetSamplingModeNid = 0x1F4011E6u;
inline constexpr std::uint32_t kCtrlReadBufferPositiveNid = 0x1F803938u;

class DisplayState;

struct ControllerSettings {
    std::int32_t idle_reset = -1;
    std::int32_t idle_back = -1;
    std::uint32_t writes{};
    std::uint32_t sampling_mode{};
    std::uint32_t cycle_writes{}, mode_writes{};
    PadSample sampled{};
    std::uint32_t sample_timestamp{};
    std::uint64_t sample_generation{}, consumed_generation{};
    std::uint32_t reads{};
    void publish_sample(PadSample pad, std::uint64_t microseconds);
};

[[nodiscard]] std::shared_ptr<ControllerSettings> register_controller_settings_hle(psprecomp::Runtime&);
void register_controller_read_hle(psprecomp::Runtime&, std::shared_ptr<ControllerSettings>,
    std::shared_ptr<DisplayState>, std::function<PadSample()>);
} // namespace azazel6
