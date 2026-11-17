#include "azazel6/bootstrap.hpp"
#include "azazel6/rtc.hpp"
#include "psprecomp/common.hpp"
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <tuple>

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void conversion_contract() {
    using namespace azazel6;
    check(rtc_tick_from_unix_microseconds(0) == static_cast<std::uint64_t>(kRtcUnixEpochOffsetUs),
        "Unix epoch must map to PSP RTC epoch");
    check(rtc_tick_from_unix_microseconds(-kRtcUnixEpochOffsetUs) == 0,
        "RTC epoch must map to zero");
    for (const auto invalid : {std::numeric_limits<std::int64_t>::min(),
             std::numeric_limits<std::int64_t>::max()}) {
        bool rejected = false;
        try { (void)rtc_tick_from_unix_microseconds(invalid); }
        catch (const psprecomp::Error&) { rejected = true; }
        check(rejected, "Out-of-range RTC calendar anchor was accepted");
    }
}

void tick_contract() {
    using namespace azazel6;
    psprecomp::Runtime rt;
    auto display = std::make_shared<DisplayState>();
    constexpr std::uint64_t anchor = 62135596800123456ULL;
    auto rtc = register_rtc_hle(rt, display, anchor);
    auto& c = rt.cpu();
    constexpr std::uint32_t address = 0x08000100u;
    auto query = [&](std::uint64_t expected) {
        c.pc = 0x08001000u; c.gpr[31] = 0x08001004u;
        c.gpr[4] = address; c.gpr[2] = 42u; c.gpr[16] = 77u;
        const auto before = c;
        const auto clock_before = display->clock.ticks();
        invoke_returning_import(rt, c, "sceRtc", 0x3F7AD767u);
        const auto tick = static_cast<std::uint64_t>(rt.memory().load32(address)) |
            (static_cast<std::uint64_t>(rt.memory().load32(address + 4u)) << 32);
        check(!rt.stopped() && tick == expected && c.gpr[2] == 0u &&
            c.pc == 0x08001004u && c.gpr[16] == before.gpr[16] &&
            display->clock.ticks() == clock_before, "RTC query changed time or return ABI");
    };
    query(anchor); query(anchor);
    display->clock.advance(17u); query(anchor);
    display->clock.advance(1u); query(anchor + 1u);
    check(rtc->reads == 4u, "RTC query accounting mismatch");
}

void arithmetic_contract() {
    using namespace azazel6;
    auto query = [](std::uint32_t nid, std::uint64_t source_tick, std::uint64_t units,
                    std::uint64_t expected, bool alias) {
        psprecomp::Runtime rt;
        auto display = std::make_shared<DisplayState>();
        auto rtc = register_rtc_hle(rt, display, 0u);
        constexpr std::uint32_t source = 0x08000100u;
        const auto destination = alias ? source : 0x08000110u;
        rt.memory().store32(source, static_cast<std::uint32_t>(source_tick));
        rt.memory().store32(source + 4u, static_cast<std::uint32_t>(source_tick >> 32));
        auto& c = rt.cpu(); c.pc = 0x08001000u; c.gpr[31] = 0x08001004u;
        c.gpr[4] = destination; c.gpr[5] = source;
        c.gpr[6] = static_cast<std::uint32_t>(units);
        c.gpr[7] = static_cast<std::uint32_t>(units >> 32);
        c.gpr[2] = 42u; c.gpr[16] = 77u;
        invoke_returning_import(rt, c, "sceRtc", nid);
        const auto actual = static_cast<std::uint64_t>(rt.memory().load32(destination)) |
            (static_cast<std::uint64_t>(rt.memory().load32(destination + 4u)) << 32);
        check(!rt.stopped() && actual == expected && c.gpr[2] == 0u &&
            c.gpr[16] == 77u && c.pc == 0x08001004u &&
            (nid == 0xF2A4AFE5u ? rtc->seconds_adds : rtc->minutes_adds) == 1u,
            "RTC arithmetic or return ABI mismatch");
    };
    query(0xF2A4AFE5u, 62135596800000000ULL, 0u, 62135596800000000ULL, false);
    query(0xF2A4AFE5u, 62135596800000000ULL, 2u, 62135596802000000ULL, false);
    query(0xF2A4AFE5u, 62135596800000000ULL, ~0ULL, 62135596799000000ULL, true);
    query(0xE6605BCAu, 62135596800000000ULL, 480u, 62135625600000000ULL, true);
    query(0xE6605BCAu, 62135596800000000ULL, ~0ULL, 62135596740000000ULL, false);
}

void set_tick_contract() {
    using namespace azazel6;
    struct Expected {
        std::uint64_t tick;
        std::uint16_t year, month, day, hour, minute, second;
        std::uint32_t microsecond;
    };
    for (const Expected value : {
            Expected{0u, 1, 1, 1, 0, 0, 0, 0},
            Expected{62135596800000000ULL, 1970, 1, 1, 0, 0, 0, 0},
            Expected{63087424496123456ULL, 2000, 2, 29, 12, 34, 56, 123456},
            Expected{315537897599999999ULL, 9999, 12, 31, 23, 59, 59, 999999}}) {
        psprecomp::Runtime rt;
        auto rtc = register_rtc_hle(rt, std::make_shared<DisplayState>(), 0u);
        constexpr std::uint32_t destination = 0x08000100u, source = 0x08000120u;
        rt.memory().store32(source, static_cast<std::uint32_t>(value.tick));
        rt.memory().store32(source + 4u, static_cast<std::uint32_t>(value.tick >> 32));
        auto& c = rt.cpu(); c.pc = 0x08001000u; c.gpr[31] = 0x08001004u;
        c.gpr[4] = destination; c.gpr[5] = source; c.gpr[2] = 42u; c.gpr[16] = 77u;
        invoke_returning_import(rt, c, "sceRtc", 0x7ED29E40u);
        check(!rt.stopped() && c.gpr[2] == 0u && c.gpr[16] == 77u &&
            c.pc == 0x08001004u && rtc->set_ticks == 1u &&
            rt.memory().load16(destination) == value.year &&
            rt.memory().load16(destination + 2u) == value.month &&
            rt.memory().load16(destination + 4u) == value.day &&
            rt.memory().load16(destination + 6u) == value.hour &&
            rt.memory().load16(destination + 8u) == value.minute &&
            rt.memory().load16(destination + 10u) == value.second &&
            rt.memory().load32(destination + 12u) == value.microsecond,
            "RTC calendar conversion or return ABI mismatch");
    }
    for (const auto [destination, source, tick] : {
            std::tuple{0x09FFFFF4u, 0x08000120u, 62135596800000000ULL},
            std::tuple{0x08000101u, 0x08000120u, 62135596800000000ULL},
            std::tuple{0x08000100u, 0x09FFFFFCu, 62135596800000000ULL},
            std::tuple{0x08000100u, 0x08000120u, 315537897600000000ULL}}) {
        psprecomp::Runtime rt;
        auto rtc = register_rtc_hle(rt, std::make_shared<DisplayState>(), 0u);
        constexpr std::uint32_t sentinel = 0x08000100u;
        for (unsigned offset = 0; offset < 16; offset += 4)
            rt.memory().store32(sentinel + offset, 0xABCD1234u);
        if (rt.memory().contains(source, 8u)) {
            rt.memory().store32(source, static_cast<std::uint32_t>(tick));
            rt.memory().store32(source + 4u, static_cast<std::uint32_t>(tick >> 32));
        }
        auto& c = rt.cpu(); c.gpr[4] = destination; c.gpr[5] = source; c.gpr[2] = 99u;
        rt.invoke_import("sceRtc", 0x7ED29E40u, c);
        check(rt.stopped() && rtc->set_ticks == 0u && c.gpr[2] == 99u,
            "Invalid RTC calendar request was accepted");
        for (unsigned offset = 0; offset < 16; offset += 4)
            check(rt.memory().load32(sentinel + offset) == 0xABCD1234u,
                "Invalid RTC calendar request partially wrote output");
    }
}

void libc_time_contract() {
    using namespace azazel6;
    constexpr std::uint64_t anchor = static_cast<std::uint64_t>(kRtcUnixEpochOffsetUs) +
        123456789123456ULL;
    psprecomp::Runtime rt;
    auto display = std::make_shared<DisplayState>();
    auto rtc = register_rtc_hle(rt, display, anchor);
    auto& c = rt.cpu();
    constexpr std::uint32_t output = 0x08000100u;
    auto query = [&](std::uint32_t destination, std::uint32_t expected) {
        c.pc = 0x08001000u; c.gpr[31] = 0x08001004u;
        c.gpr[4] = destination; c.gpr[2] = 42u; c.gpr[16] = 77u;
        invoke_returning_import(rt, c, "UtilsForUser", 0x27CC57F0u);
        check(!rt.stopped() && c.gpr[2] == expected && c.gpr[16] == 77u &&
            c.pc == 0x08001004u, "libc time return ABI mismatch");
    };
    rt.memory().store32(output, 0xABCD1234u);
    query(0u, 123456789u);
    check(rt.memory().load32(output) == 0xABCD1234u, "NULL libc time wrote guest memory");
    query(output, 123456789u);
    check(rt.memory().load32(output) == 123456789u, "libc time output mismatch");
    display->clock.advance(18u);
    query(output, 123456789u);
    check(rtc->libc_time_reads == 3u, "libc time accounting mismatch");
    for (const auto invalid : {0x08000101u, 0x09FFFFFEu, 0xDEADBEEFu}) {
        rt.memory().store32(output, 0xABCD1234u);
        query(invalid, 0u);
        check(rt.memory().load32(output) == 0xABCD1234u && rtc->libc_time_reads == 3u,
            "invalid libc time pointer wrote memory or counted a read");
    }
    for (const auto bad_anchor : {static_cast<std::uint64_t>(kRtcUnixEpochOffsetUs) - 1u,
            static_cast<std::uint64_t>(kRtcUnixEpochOffsetUs) +
                (static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()) + 1u) * 1000000u,
            std::numeric_limits<std::uint64_t>::max()}) {
        psprecomp::Runtime bad_rt;
        auto bad_display = std::make_shared<DisplayState>();
        auto bad_state = register_rtc_hle(bad_rt, bad_display, bad_anchor);
        if (bad_anchor == std::numeric_limits<std::uint64_t>::max())
            bad_display->clock.advance(18u);
        auto& bad_cpu = bad_rt.cpu(); bad_cpu.gpr[4] = output; bad_cpu.gpr[2] = 99u;
        bad_rt.memory().store32(output, 0xABCD1234u);
        bad_rt.invoke_import("UtilsForUser", 0x27CC57F0u, bad_cpu);
        check(bad_rt.stopped() && bad_cpu.gpr[2] == 99u &&
            bad_rt.memory().load32(output) == 0xABCD1234u &&
            bad_state->libc_time_reads == 0u,
            "invalid libc time anchor changed guest state");
    }
}

void failure_contracts() {
    using namespace azazel6;
    for (const auto address : {0u, 0x08000101u, 0x09FFFFFCu}) {
        psprecomp::Runtime rt;
        auto display = std::make_shared<DisplayState>();
        auto rtc = register_rtc_hle(rt, display, 123u);
        if (address == 0x09FFFFFCu) rt.memory().store32(address, 0xABCD1234u);
        auto& c = rt.cpu(); c.pc = 0x08001000u; c.gpr[4] = address;
        c.gpr[2] = 99u;
        rt.invoke_import("sceRtc", 0x3F7AD767u, c);
        check(rt.stopped() && rtc->reads == 0u && c.gpr[2] == 99u,
            "Invalid RTC guest buffer was accepted");
        if (address == 0x09FFFFFCu)
            check(rt.memory().load32(address) == 0xABCD1234u,
                "Partial RTC write escaped range validation");
    }
    psprecomp::Runtime rt;
    auto display = std::make_shared<DisplayState>();
    auto rtc = register_rtc_hle(rt, display, std::numeric_limits<std::uint64_t>::max());
    display->clock.advance(18u);
    auto& c = rt.cpu(); c.gpr[4] = 0x08000100u; c.gpr[2] = 99u;
    rt.memory().store32(c.gpr[4], 0xABCD1234u);
    rt.invoke_import("sceRtc", 0x3F7AD767u, c);
    check(rt.stopped() && rtc->reads == 0u && c.gpr[2] == 99u &&
        rt.memory().load32(c.gpr[4]) == 0xABCD1234u,
        "RTC tick overflow mutated guest state");

    auto invalid_arithmetic = [](std::uint32_t nid, std::uint32_t destination, std::uint32_t source,
                                 std::uint64_t source_tick, std::uint64_t units) {
        psprecomp::Runtime runtime;
        auto display_state = std::make_shared<DisplayState>();
        auto rtc_state = register_rtc_hle(runtime, display_state, 0u);
        constexpr std::uint32_t sentinel = 0x08000120u;
        runtime.memory().store32(sentinel, 0xABCD1234u);
        if (runtime.memory().contains(source, 8u)) {
            runtime.memory().store32(source, static_cast<std::uint32_t>(source_tick));
            runtime.memory().store32(source + 4u, static_cast<std::uint32_t>(source_tick >> 32));
        }
        auto& context = runtime.cpu(); context.gpr[4] = destination; context.gpr[5] = source;
        context.gpr[6] = static_cast<std::uint32_t>(units);
        context.gpr[7] = static_cast<std::uint32_t>(units >> 32);
        context.gpr[2] = 99u;
        runtime.invoke_import("sceRtc", nid, context);
        check(runtime.stopped() && rtc_state->seconds_adds == 0u &&
            rtc_state->minutes_adds == 0u && context.gpr[2] == 99u &&
            runtime.memory().load32(sentinel) == 0xABCD1234u,
            "Invalid RTC arithmetic mutated guest state");
    };
    invalid_arithmetic(0xF2A4AFE5u, 0x09FFFFFCu, 0x08000100u, 10u, 0u);
    invalid_arithmetic(0xE6605BCAu, 0x08000120u, 0x09FFFFFCu, 10u, 0u);
    invalid_arithmetic(0xE6605BCAu, 0x08000121u, 0x08000100u, 10u, 0u);
    invalid_arithmetic(0xF2A4AFE5u, 0x08000120u, 0x08000100u, 0u, ~0ULL);
    invalid_arithmetic(0xE6605BCAu, 0x08000120u, 0x08000100u, 0u, ~0ULL);
    invalid_arithmetic(0xF2A4AFE5u, 0x08000120u, 0x08000100u,
        std::numeric_limits<std::uint64_t>::max(), 1u);
    invalid_arithmetic(0xE6605BCAu, 0x08000120u, 0x08000100u,
        std::numeric_limits<std::uint64_t>::max(), 1u);
    invalid_arithmetic(0xF2A4AFE5u, 0x08000120u, 0x08000100u, 10u,
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()));
}
}

int main() {
    try {
        conversion_contract(); tick_contract(); arithmetic_contract();
        set_tick_contract(); libc_time_contract(); failure_contracts();
        std::cout << "RTC contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
