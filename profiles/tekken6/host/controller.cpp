#include "azazel6/controller.hpp"

namespace azazel6 {
std::shared_ptr<ControllerSettings> register_controller_settings_hle(psprecomp::Runtime& rt) {
    auto state = std::make_shared<ControllerSettings>();
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
} // namespace azazel6
