#include "azazel6/controller.hpp"
#include "azazel6/display.hpp"
#include "psprecomp/common.hpp"

namespace azazel6 {
void ControllerSettings::publish_sample(PadSample pad, std::uint64_t microseconds) {
    sampled = pad;
    sample_timestamp = static_cast<std::uint32_t>(microseconds);
    ++sample_generation;
}
std::shared_ptr<ControllerSettings> register_controller_settings_hle(psprecomp::Runtime& rt) {
    auto state = std::make_shared<ControllerSettings>();
    rt.nids().add("sceCtrl", kCtrlSetSamplingCycleNid, "sceCtrlSetSamplingCycle");
    rt.register_hle("sceCtrl", kCtrlSetSamplingCycleNid, [state](auto& runtime, auto& c) {
        const auto cycle = c.gpr[4];
        if (cycle && (cycle < 5555 || cycle > 20000)) {
            c.set_gpr(2, 0x800001FEu); return;
        }
        if (cycle) { runtime.stop("Timed controller sampling is not implemented"); return; }
        // Only VBLANK sampling is configured. No sample or elapsed time is invented.
        ++state->cycle_writes;
        c.set_gpr(2, 0);
    });
    rt.nids().add("sceCtrl", kCtrlSetSamplingModeNid, "sceCtrlSetSamplingMode");
    rt.register_hle("sceCtrl", kCtrlSetSamplingModeNid, [state](auto&, auto& c) {
        const auto mode = c.gpr[4];
        if (mode > 1) { c.set_gpr(2, 0x80000107u); return; }
        c.set_gpr(2, state->sampling_mode);
        state->sampling_mode = mode;
        ++state->mode_writes;
    });
    rt.nids().add("sceCtrl", kCtrlSetIdleCancelThresholdNid, "sceCtrlSetIdleCancelThreshold");
    rt.register_hle("sceCtrl", kCtrlSetIdleCancelThresholdNid, [state](auto&, auto& c) {
        constexpr std::uint32_t invalid_value = 0x800001FEu;
        const auto reset = static_cast<std::int32_t>(c.gpr[4]);
        const auto back = static_cast<std::int32_t>(c.gpr[5]);
        if (reset < -1 || reset > 128 || back < -1 || back > 128) {
            c.set_gpr(2, invalid_value); return;
        }
        state->idle_reset = reset; state->idle_back = back; ++state->writes;
        c.set_gpr(2, 0);
    });
    return state;
}
void register_controller_read_hle(psprecomp::Runtime& rt, std::shared_ptr<ControllerSettings> state,
    std::shared_ptr<DisplayState> display, std::function<PadSample()> sample_host) {
    if (!state || !display || !sample_host) throw psprecomp::Error("Controller sampling requires owned state and input");
    display->set_vblank_observer([state, sample_host = std::move(sample_host)](std::uint64_t time) {
        state->publish_sample(sample_host(), time);
    });
    rt.nids().add("sceCtrl", kCtrlReadBufferPositiveNid, "sceCtrlReadBufferPositive");
    rt.register_hle("sceCtrl", kCtrlReadBufferPositiveNid, [state](auto& runtime, auto& c) {
        const auto address = c.gpr[4], count = c.gpr[5];
        if (!count || count >= 64) { c.set_gpr(2, 0x80000104u); return; }
        if (count != 1) { runtime.stop("Multi-sample controller read is not implemented"); return; }
        const auto end = static_cast<std::uint64_t>(address) + 16u;
        if ((address & 3u) || address < psprecomp::GuestMemory::kPhysicalBase ||
            end > static_cast<std::uint64_t>(psprecomp::GuestMemory::kPhysicalBase) + runtime.memory().size()) {
            c.set_gpr(2, 0x80000023u); return;
        }
        if (state->sample_generation == state->consumed_generation) {
            runtime.stop("Controller read requires next VBLANK sample"); return;
        }
        auto& memory = runtime.memory();
        memory.store32(address, state->sample_timestamp);
        constexpr std::uint32_t user_buttons = azazel6::select | start | up | right | down | left |
            ltrigger | rtrigger | triangle | circle | cross | square;
        memory.store32(address + 4, state->sampled.buttons & user_buttons);
        memory.store8(address + 8, state->sampling_mode ? state->sampled.x : 128);
        memory.store8(address + 9, state->sampling_mode ? state->sampled.y : 128);
        for (std::uint32_t offset = 10; offset < 16; ++offset) memory.store8(address + offset, 0);
        state->consumed_generation = state->sample_generation;
        ++state->reads;
        c.set_gpr(2, 1);
    });
}
} // namespace azazel6
