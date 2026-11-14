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

namespace {
void register_tick_add(psprecomp::Runtime& runtime, std::shared_ptr<RtcState> state,
    std::uint32_t nid, std::uint64_t microseconds_per_unit,
    std::uint64_t RtcState::* counter) {
    runtime.register_hle("sceRtc", nid, [state, microseconds_per_unit, counter](auto& rt, auto& ctx) {
        const auto destination = ctx.gpr[4], source = ctx.gpr[5];
        auto& memory = rt.memory();
        if ((destination & 3u) != 0u || (source & 3u) != 0u ||
            !memory.contains(destination, 8u) || !memory.contains(source, 8u)) {
            rt.stop("RTC arithmetic buffer outside guest memory"); return;
        }
        const auto units = static_cast<std::uint64_t>(ctx.gpr[6]) |
            (static_cast<std::uint64_t>(ctx.gpr[7]) << 32);
        const bool negative = (units >> 63) != 0u;
        const auto magnitude = negative ? (~units + 1u) : units;
        if (magnitude > std::numeric_limits<std::uint64_t>::max() / microseconds_per_unit) {
            rt.stop("RTC arithmetic delta overflow"); return;
        }
        const auto delta = magnitude * microseconds_per_unit;
        const auto tick = static_cast<std::uint64_t>(memory.load32(source)) |
            (static_cast<std::uint64_t>(memory.load32(source + 4u)) << 32);
        if ((negative && tick < delta) ||
            (!negative && tick > std::numeric_limits<std::uint64_t>::max() - delta)) {
            rt.stop("RTC arithmetic result overflow"); return;
        }
        const auto result = negative ? tick - delta : tick + delta;
        memory.store32(destination, static_cast<std::uint32_t>(result));
        memory.store32(destination + 4u, static_cast<std::uint32_t>(result >> 32));
        ctx.set_gpr(2, 0u);
        ++(state.get()->*counter);
    });
}
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
    register_tick_add(runtime, state, 0xF2A4AFE5u, 1000000u, &RtcState::seconds_adds);
    register_tick_add(runtime, state, 0xE6605BCAu, 60000000u, &RtcState::minutes_adds);
    return state;
}
}
