#include "azazel6/rtc.hpp"
#include "psprecomp/common.hpp"
#include <chrono>
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
    runtime.register_hle("sceRtc", 0x7ED29E40u, [state](auto& rt, auto& ctx) {
        constexpr std::uint64_t day_microseconds = 86400000000ULL;
        constexpr std::int64_t days_to_unix_epoch = 719162;
        const auto date_address = ctx.gpr[4], tick_address = ctx.gpr[5];
        auto& memory = rt.memory();
        if ((date_address & 3u) != 0u || (tick_address & 3u) != 0u ||
            !memory.contains(date_address, 16u) || !memory.contains(tick_address, 8u)) {
            rt.stop("RTC date or tick buffer outside guest memory"); return;
        }
        const auto tick = static_cast<std::uint64_t>(memory.load32(tick_address)) |
            (static_cast<std::uint64_t>(memory.load32(tick_address + 4u)) << 32);
        const auto day_index = tick / day_microseconds;
        const auto last_day = static_cast<std::uint64_t>(
            (std::chrono::sys_days{std::chrono::year{9999}/12/31} -
             std::chrono::sys_days{std::chrono::year{1970}/1/1}).count() + days_to_unix_epoch);
        if (day_index > last_day) { rt.stop("RTC tick outside supported calendar"); return; }
        const auto date = std::chrono::year_month_day{std::chrono::sys_days{
            std::chrono::days{static_cast<std::int64_t>(day_index) - days_to_unix_epoch}}};
        if (!date.ok() || static_cast<int>(date.year()) < 1) {
            rt.stop("RTC tick outside supported calendar"); return;
        }
        const auto remainder = tick % day_microseconds;
        memory.store16(date_address, static_cast<std::uint16_t>(static_cast<int>(date.year())));
        memory.store16(date_address + 2u, static_cast<std::uint16_t>(static_cast<unsigned>(date.month())));
        memory.store16(date_address + 4u, static_cast<std::uint16_t>(static_cast<unsigned>(date.day())));
        memory.store16(date_address + 6u, static_cast<std::uint16_t>(remainder / 3600000000ULL));
        memory.store16(date_address + 8u, static_cast<std::uint16_t>((remainder / 60000000ULL) % 60u));
        memory.store16(date_address + 10u, static_cast<std::uint16_t>((remainder / 1000000ULL) % 60u));
        memory.store32(date_address + 12u, static_cast<std::uint32_t>(remainder % 1000000ULL));
        ctx.set_gpr(2, 0u);
        ++state->set_ticks;
    });
    runtime.register_hle("UtilsForUser", 0x27CC57F0u,
        [state, display, anchor_tick](auto& rt, auto& ctx) {
            const auto destination = ctx.gpr[4];
            if (destination != 0u && ((destination & 3u) != 0u ||
                !rt.memory().contains(destination, 4u))) {
                // The firmware returns zero for an invalid optional time_t pointer.
                ctx.set_gpr(2, 0u);
                return;
            }
            const auto elapsed = display->clock.microseconds();
            if (elapsed > std::numeric_limits<std::uint64_t>::max() - anchor_tick) {
                rt.stop("libc time tick overflow"); return;
            }
            const auto tick = anchor_tick + elapsed;
            if (tick < static_cast<std::uint64_t>(kRtcUnixEpochOffsetUs)) {
                rt.stop("libc time before Unix epoch"); return;
            }
            const auto seconds = (tick - static_cast<std::uint64_t>(kRtcUnixEpochOffsetUs)) / 1000000u;
            if (seconds > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max())) {
                rt.stop("libc time outside signed 32-bit range"); return;
            }
            if (destination != 0u) rt.memory().store32(destination, static_cast<std::uint32_t>(seconds));
            ctx.set_gpr(2, static_cast<std::uint32_t>(seconds));
            ++state->libc_time_reads;
        });
    return state;
}
}
