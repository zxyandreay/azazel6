#include "azazel6/bootstrap.hpp"
#include "azazel6/power.hpp"
#include <iostream>
#include <stdexcept>

using psprecomp::Runtime;
using namespace azazel6;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void nominal_query_contracts() {
    auto rt = std::make_unique<Runtime>();
    auto state = register_power_clock_hle(*rt);
    auto& ctx = rt->cpu();
    const struct Query { std::uint32_t nid; std::uint32_t expected; } queries[] = {
        {kPowerGetPllClockFrequencyIntNid, 222},
        {kPowerGetCpuClockFrequencyIntNid, 222},
        {kPowerGetBusClockFrequencyIntNid, 111},
    };
    for (const auto query : queries) {
        ctx.gpr.fill(0xA5A5A5A5u);
        ctx.gpr[0] = 0;
        ctx.pc = 0x08804000u;
        ctx.gpr[31] = 0x08805000u;
        const auto before = ctx.gpr;
        invoke_returning_import(*rt, ctx, "scePower", query.nid);
        require(!rt->stopped() && ctx.pc == 0x08805000u && ctx.gpr[2] == query.expected,
                "Power query return ABI");
        for (std::size_t i = 0; i < before.size(); ++i)
            if (i != 2) require(ctx.gpr[i] == before[i], "Power query clobbered a register");
    }
    require(state->pll_mhz == 222 && state->cpu_mhz == 222 && state->bus_mhz == 111,
            "Power query changed nominal clocks");
    require(state->pll_reads == 1 && state->cpu_reads == 1 && state->bus_reads == 1,
            "Power query accounting");
}

void runtime_ownership_contract() {
    auto first = std::make_unique<Runtime>();
    auto first_state = register_power_clock_hle(*first);
    first->invoke_import("scePower", kPowerGetPllClockFrequencyIntNid, first->cpu());
    auto second = std::make_unique<Runtime>();
    auto second_state = register_power_clock_hle(*second);
    require(first_state->pll_reads == 1 && second_state->pll_reads == 0,
            "Power state leaked across runtimes");
    first_state.reset();
    first->invoke_import("scePower", kPowerGetPllClockFrequencyIntNid, first->cpu());
    require(!first->stopped(), "Runtime did not retain power state");
}

int main() {
    try {
        nominal_query_contracts(); runtime_ownership_contract();
        std::cout << "Power clock query contracts passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
