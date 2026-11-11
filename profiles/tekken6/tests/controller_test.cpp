#include "azazel6/bootstrap.hpp"
#include "azazel6/controller.hpp"
#include <iostream>
#include <stdexcept>

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

int main() {
    try { threshold_contracts(); sampling_settings(); std::cout << "Controller settings contracts passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
