#include "azazel6/power.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>

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

void register_power_callback_hle(psprecomp::Runtime& rt, std::shared_ptr<PowerClocks> state,
                                 std::shared_ptr<ThreadRegistry> threads) {
    if (!state || !threads) throw psprecomp::Error("Power callbacks require owned clock and thread state");
    rt.nids().add("scePower", kPowerRegisterCallbackNid, "scePowerRegisterCallback");
    rt.register_hle("scePower", kPowerRegisterCallbackNid,
        [state, threads](auto& runtime, auto& c) {
            constexpr std::uint32_t invalid_slot = 0x80000102u;
            constexpr std::uint32_t invalid_callback = 0x80000100u;
            constexpr std::uint32_t taken_slot = 0x80000020u;
            constexpr std::uint32_t slots_full = 0x80000022u;
            const auto slot = static_cast<std::int32_t>(c.gpr[4]);
            if (slot < -1 || slot >= static_cast<std::int32_t>(state->callback_slots.size())) {
                c.set_gpr(2, invalid_slot); return;
            }
            const auto current = threads->current_thread();
            if (!current || current->status != ThreadStatus::Running || !threads->interrupts_enabled()) {
                runtime.stop("Power callback registration requires an interruptible running thread"); return;
            }
            const auto callback = threads->callbacks().find(c.gpr[5]);
            if (callback == threads->callbacks().end() || callback->second.owner != current->uid) {
                c.set_gpr(2, invalid_callback); return;
            }
            std::size_t selected{};
            if (slot == -1) {
                const auto free = std::find(state->callback_slots.begin(), state->callback_slots.end(), std::nullopt);
                if (free == state->callback_slots.end()) { c.set_gpr(2, slots_full); return; }
                selected = static_cast<std::size_t>(free - state->callback_slots.begin());
            } else {
                selected = static_cast<std::size_t>(slot);
                if (state->callback_slots[selected]) { c.set_gpr(2, taken_slot); return; }
            }
            state->callback_slots[selected] = c.gpr[5];
            // PSP registration notifies the callback of the current power state.
            // Retain that work explicitly until general guest callback delivery exists.
            state->pending_notifications.push_back({c.gpr[5], 0x000010E4u});
            c.set_gpr(2, slot == -1 ? static_cast<std::uint32_t>(selected) : 0u);
        });
}
} // namespace azazel6
