#include "azazel6/utility_modules.hpp"

namespace azazel6 {
std::shared_ptr<StartupAvModules> register_startup_av_modules_hle(psprecomp::Runtime& rt) {
    auto state = std::make_shared<StartupAvModules>();
    constexpr std::uint32_t nid = 0x2A2B3DE0u;
    constexpr std::uint32_t already_loaded = 0x80111102u;
    rt.nids().add("sceUtility", nid, "sceUtilityLoadModule");
    rt.register_hle("sceUtility", nid, [state](auto& runtime, auto& c) {
        const auto id = c.gpr[4];
        if (&c != &runtime.cpu() || id < 0x300u || id > 0x302u) {
            runtime.stop("Unsupported utility module load"); return;
        }
        const auto bit = 1u << (id - 0x300u);
        if (state->loaded_mask & bit) {
            ++state->duplicate_loads;
            c.set_gpr(2, already_loaded);
            return;
        }
        state->loaded_mask |= bit;
        ++state->successful_loads;
        c.set_gpr(2, 0u);
    });
    return state;
}
}
