#pragma once
#include "azazel6/display.hpp"
#include <cstdint>
#include <memory>

namespace azazel6 {
constexpr std::int64_t kRtcUnixEpochOffsetUs = 62135596800000000LL;

// Calendar date is anchored once; elapsed time comes only from explicit guest events.
std::uint64_t rtc_tick_from_unix_microseconds(std::int64_t unix_microseconds);

struct RtcState {
    std::uint64_t reads = 0;
    std::uint64_t seconds_adds = 0;
    std::uint64_t minutes_adds = 0;
};

std::shared_ptr<RtcState> register_rtc_hle(psprecomp::Runtime& runtime,
    std::shared_ptr<DisplayState> display, std::uint64_t anchor_tick);
}
