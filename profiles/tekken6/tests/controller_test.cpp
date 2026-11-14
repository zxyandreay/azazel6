#include "azazel6/bootstrap.hpp"
#include "azazel6/controller.hpp"
#include "azazel6/display.hpp"
#include <iostream>
#include <stdexcept>
#include <tuple>

using namespace azazel6;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

std::uint32_t set_threshold(psprecomp::Runtime& rt, std::int32_t reset, std::int32_t back) {
    auto& c = rt.cpu(); c.gpr[4] = static_cast<std::uint32_t>(reset);
    c.gpr[5] = static_cast<std::uint32_t>(back); c.pc = 0x08804000u; c.gpr[31] = 0x08805000u;
    invoke_returning_import(rt, c, "sceCtrl", kCtrlSetIdleCancelThresholdNid);
    require(c.pc == 0x08805000u, "Controller setting return ABI");
    return c.gpr[2];
}

void threshold_contracts() {
    auto rt = std::make_unique<psprecomp::Runtime>();
    const auto state = register_controller_settings_hle(*rt);
    require(state->idle_reset == -1 && state->idle_back == -1 && state->writes == 0,
            "Controller setting defaults");
    for (const auto values : {std::pair{-1, -1}, {0, 128}, {128, 0}, {64, -1}}) {
        require(set_threshold(*rt, values.first, values.second) == 0 &&
                state->idle_reset == values.first && state->idle_back == values.second,
                "Valid controller thresholds");
    }
    require(state->writes == 4, "Controller setting accounting");
    for (const auto values : {std::pair{-2, 0}, {0, -2}, {129, 0}, {0, 129}}) {
        const auto before = *state;
        require(set_threshold(*rt, values.first, values.second) == 0x800001FEu &&
                state->idle_reset == before.idle_reset && state->idle_back == before.idle_back &&
                state->writes == before.writes, "Invalid controller thresholds mutated state");
    }
    auto other = std::make_unique<psprecomp::Runtime>();
    const auto isolated = register_controller_settings_hle(*other);
    require(isolated->writes == 0 && isolated->idle_reset == -1 && state->writes == 4,
            "Controller settings leaked across runtimes");
}

void sampling_settings() {
    auto rt = std::make_unique<psprecomp::Runtime>();
    const auto state = register_controller_settings_hle(*rt);
    auto call = [&](std::uint32_t nid, std::uint32_t value) {
        auto& c = rt->cpu(); c.gpr[4] = value; c.gpr[2] = 0xDEADBEEFu;
        c.pc = 0x08804000u; c.gpr[31] = 0x08805000u;
        const auto before = c.gpr;
        invoke_returning_import(*rt, c, "sceCtrl", nid);
        require(!rt->stopped() && c.pc == c.gpr[31], "Sampling setting return");
        for (unsigned i = 0; i < 32; ++i)
            if (i != 2) require(c.gpr[i] == before[i], "Sampling setting clobbered register");
        return c.gpr[2];
    };
    require(call(kCtrlSetSamplingCycleNid, 0) == 0 && call(kCtrlSetSamplingCycleNid, 0) == 0 &&
            state->cycle_writes == 2, "VBLANK sampling configuration");
    for (auto cycle : {1u, 5554u, 20001u, 0xFFFFFFFFu})
        require(call(kCtrlSetSamplingCycleNid, cycle) == 0x800001FEu && state->cycle_writes == 2,
                "Invalid cycle changed settings");
    for (auto mode : {1u, 1u, 0u, 1u}) {
        const auto previous = state->sampling_mode;
        require(call(kCtrlSetSamplingModeNid, mode) == previous && state->sampling_mode == mode,
                "Sampling mode previous value");
    }
    for (auto mode : {2u, 255u, 0xFFFFFFFFu})
        require(call(kCtrlSetSamplingModeNid, mode) == 0x80000107u &&
                state->sampling_mode == 1 && state->mode_writes == 4, "Invalid mode changed state");
    for (auto cycle : {5555u, 10000u, 20000u}) {
        auto other = std::make_unique<psprecomp::Runtime>();
        const auto isolated = register_controller_settings_hle(*other);
        auto& c = other->cpu(); c.gpr[4] = cycle; c.gpr[2] = 0xDEADBEEFu;
        c.pc = 0x08804000u; c.gpr[31] = 0x08805000u;
        invoke_returning_import(*other, c, "sceCtrl", kCtrlSetSamplingCycleNid);
        require(other->stopped() && c.pc == 0x08804000u && c.gpr[2] == 0xDEADBEEFu &&
                !isolated->sampling_mode && !isolated->cycle_writes && !isolated->mode_writes,
                "Unsupported timer sampling succeeded or leaked settings");
    }
}

void positive_read_contracts() {
    auto call = [](auto& rt, std::uint32_t address, std::uint32_t count) {
        auto& c = rt.cpu(); c.gpr[4] = address; c.gpr[5] = count; c.gpr[2] = 0xDEADBEEFu;
        c.pc = 0x08804000u; c.gpr[31] = 0x08805000u;
        invoke_returning_import(rt, c, "sceCtrl", kCtrlReadBufferPositiveNid);
        return c.gpr[2];
    };
    auto rt = std::make_unique<psprecomp::Runtime>();
    const auto state = register_controller_settings_hle(*rt);
    const auto display = std::make_shared<DisplayState>();
    register_controller_read_hle(*rt, state, display, [] { return PadSample{cross | right, 21, 239}; });
    state->sampling_mode = 1;
    state->publish_sample({cross | right, 21, 239}, 16683);
    constexpr auto address = 0x08010000u;
    require(call(*rt, address, 1) == 1 && !rt->stopped() && rt->cpu().pc == 0x08805000u &&
            rt->memory().load32(address) == 16683 &&
            rt->memory().load32(address + 4) == (cross | right) &&
            rt->memory().load8(address + 8) == 21 && rt->memory().load8(address + 9) == 239 &&
            rt->memory().load32(address + 12) == 0 && state->reads == 1,
            "VBLANK controller sample layout or return ABI");
    require(call(*rt, address, 1) == 0xDEADBEEFu && rt->stopped() && state->reads == 1,
            "Controller read reused an already consumed sample");
    for (const auto [buffer, count, error] : {
             std::tuple{0u, 1u, 0x80000023u},
             std::tuple{address + 1, 1u, 0x80000023u},
             std::tuple{0xFFFFFFFFu, 1u, 0x80000023u},
             std::tuple{address, 0u, 0x80000104u},
             std::tuple{address, 64u, 0x80000104u}}) {
        auto invalid = std::make_unique<psprecomp::Runtime>();
        const auto settings = register_controller_settings_hle(*invalid);
        register_controller_read_hle(*invalid, settings, std::make_shared<DisplayState>(), [] { return PadSample{}; });
        settings->publish_sample({}, 1);
        require(call(*invalid, buffer, count) == error && !invalid->stopped() && !settings->reads &&
                settings->sample_generation != settings->consumed_generation,
                "Invalid controller read consumed sample or accepted bad buffer");
    }
    auto digital = std::make_unique<psprecomp::Runtime>();
    const auto settings = register_controller_settings_hle(*digital);
    register_controller_read_hle(*digital, settings, std::make_shared<DisplayState>(), [] { return PadSample{}; });
    settings->publish_sample({start | 0x01000000u, 0, 255}, 42);
    require(call(*digital, address, 1) == 1 && digital->memory().load32(address + 4) == start &&
            digital->memory().load8(address + 8) == 128 && digital->memory().load8(address + 9) == 128,
            "Digital sampling leaked analog values");
}

int main() {
    try { threshold_contracts(); sampling_settings(); positive_read_contracts(); std::cout << "Controller settings contracts passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
