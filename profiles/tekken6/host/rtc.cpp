#include "azazel6/rtc.hpp"
#include "psprecomp/common.hpp"
#include <limits>

namespace azazel6 {
std::uint64_t rtc_tick_from_unix_microseconds(std::int64_t unix_microseconds) {
    if (unix_microseconds < -kRtcUnixEpochOffsetUs ||
        unix_microseconds > std::numeric_limits<std::int64_t>::max() - kRtcUnixEpochOffsetUs)
        throw psprecomp::Error("RTC calendar anchor outside supported range");
    return static_cast<std::uint64_t>(unix_microseconds + kRtcUnixEpochOffsetUs);
}

std::shared_ptr<RtcState> register_rtc_hle(psprecomp::Runtime& runtime,
    std::shared_ptr<DisplayState> display, std::uint64_t anchor_tick) {
    if (!display) throw psprecomp::Error("RTC requires an owned guest event clock");
    auto state = std::make_shared<RtcState>();
    runtime.register_hle("sceRtc", 0x3F7AD767u, [state, display, anchor_tick](auto& rt, auto& ctx) {
        const auto address = ctx.gpr[4];
        if ((address & 3u) != 0u || !rt.memory().contains(address, 8u)) {
            rt.stop("RTC tick destination outside guest memory"); return;
        }
        const auto elapsed = display->clock.microseconds();
        if (elapsed > std::numeric_limits<std::uint64_t>::max() - anchor_tick) {
            rt.stop("RTC tick overflow"); return;
        }
        const auto tick = anchor_tick + elapsed;
        rt.memory().store32(address, static_cast<std::uint32_t>(tick));
        rt.memory().store32(address + 4u, static_cast<std::uint32_t>(tick >> 32));
        ctx.set_gpr(2, 0u);
        ++state->reads;
    });
    return state;
}
}
