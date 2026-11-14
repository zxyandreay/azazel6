#include "azazel6/bootstrap.hpp"
#include "azazel6/rtc.hpp"
#include "psprecomp/common.hpp"
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>

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
}
}

int main() {
    try {
        conversion_contract(); tick_contract(); failure_contracts();
        std::cout << "RTC contracts passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
