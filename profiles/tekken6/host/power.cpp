#include "azazel6/power.hpp"

namespace azazel6 {
std::shared_ptr<PowerClocks> register_power_clock_hle(psprecomp::Runtime& rt) {
    auto state = std::make_shared<PowerClocks>();
    rt.nids().add("scePower", kPowerGetPllClockFrequencyIntNid,
                  "scePowerGetPllClockFrequencyInt");
    rt.nids().add("scePower", kPowerGetCpuClockFrequencyIntNid,
                  "scePowerGetCpuClockFrequencyInt");
    rt.nids().add("scePower", kPowerGetBusClockFrequencyIntNid,
                  "scePowerGetBusClockFrequencyInt");
    rt.register_hle("scePower", kPowerGetPllClockFrequencyIntNid,
        [state](auto&, auto& c) { ++state->pll_reads; c.set_gpr(2, state->pll_mhz); });
    rt.register_hle("scePower", kPowerGetCpuClockFrequencyIntNid,
        [state](auto&, auto& c) { ++state->cpu_reads; c.set_gpr(2, state->cpu_mhz); });
    rt.register_hle("scePower", kPowerGetBusClockFrequencyIntNid,
        [state](auto&, auto& c) { ++state->bus_reads; c.set_gpr(2, state->bus_mhz); });
    return state;
}
} // namespace azazel6
