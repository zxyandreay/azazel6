#include "azazel6/settings.hpp"
#include "psprecomp/common.hpp"

namespace azazel6 {
std::shared_ptr<LanguageMode> register_language_hle(psprecomp::Runtime& rt, LanguageMode initial,
                                                  std::uint32_t begin, std::uint32_t end) {
    if (initial.language > 11 || initial.confirm_button > 1 || begin >= end ||
        begin < psprecomp::GuestMemory::kPhysicalBase || ((begin | end) & 3u) ||
        static_cast<std::uint64_t>(end) > psprecomp::GuestMemory::kPhysicalBase +
            static_cast<std::uint64_t>(rt.memory().size()))
        throw psprecomp::Error("Invalid language profile or writable data range");
    auto state = std::make_shared<LanguageMode>(initial);
    state->reads = state->writes = 0;
    rt.nids().add("sceImpose", 0x24FD7BCFu, "sceImposeGetLanguageMode");
    rt.nids().add("sceImpose", 0x36AA6E91u, "sceImposeSetLanguageMode");
    rt.register_hle("sceImpose", 0x24FD7BCFu, [state, begin, end](auto& runtime, auto& c) {
        // Preflight both writes so a bad second pointer cannot partly change RAM.
        for (auto address : {c.gpr[4], c.gpr[5]}) {
            if ((address & 3u) || address < begin || static_cast<std::uint64_t>(address) + 4 > end ||
                runtime.has_function(address)) {
                runtime.stop("Language output outside verified writable startup data"); return;
            }
        }
        runtime.memory().store32(c.gpr[4], state->language);
        runtime.memory().store32(c.gpr[5], state->confirm_button);
        ++state->reads; c.set_gpr(2, 0);
    });
    rt.register_hle("sceImpose", 0x36AA6E91u, [state](auto& runtime, auto& c) {
        if (c.gpr[4] > 11 || c.gpr[5] > 1) {
            runtime.stop("Language mode outside verified profile values"); return;
        }
        state->language = c.gpr[4]; state->confirm_button = c.gpr[5];
        ++state->writes; c.set_gpr(2, 0);
    });
    return state;
}
}
