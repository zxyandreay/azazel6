#include "azazel6/settings.hpp"
#include "azazel6/threads.hpp"
#include "azazel6/memory.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>

namespace azazel6 {
void register_system_param_hle(psprecomp::Runtime& rt, std::shared_ptr<ThreadRegistry> threads,
                               std::uint32_t confirm_button, std::int32_t timezone_minutes,
                               std::shared_ptr<const PartitionMemory> partition) {
    if (!threads || !threads->belongs_to(rt) || confirm_button > 1 ||
        timezone_minutes < -840 || timezone_minutes > 840 ||
        (partition && partition->owner != &rt))
        throw psprecomp::Error("Invalid system settings or owner");
    constexpr std::uint32_t get_int = 0xA5DA2406u;
    rt.nids().add("sceUtility", get_int, "sceUtilityGetSystemParamInt");
    rt.register_hle("sceUtility", get_int, [threads, confirm_button, timezone_minutes, partition](auto& runtime, auto& c) {
        const auto id = c.gpr[4];
        if (id != 6u && id != 9u) { runtime.stop("Unsupported integer system parameter"); return; }
        const auto caller = threads->current_thread();
        const auto out = c.gpr[5];
        const auto finish = static_cast<std::uint64_t>(out) + 4u;
        const bool stack = caller && out >= caller->stack_base &&
            finish <= static_cast<std::uint64_t>(caller->stack_base) + caller->stack_size;
        const bool block = id == 6u && partition && std::any_of(partition->blocks.begin(), partition->blocks.end(),
            [out, finish](const auto& item) {
                const auto& allocation = item.second;
                return out >= allocation.base &&
                    finish <= static_cast<std::uint64_t>(allocation.base) + allocation.size;
            });
        if (&c != &runtime.cpu() || !caller || caller->status != ThreadStatus::Running ||
            threads->in_interrupt() || !threads->interrupts_enabled() || (out & 3u) ||
            !runtime.memory().contains(out, 4u) || (!stack && !block)) {
            runtime.stop("System parameter output outside live caller memory"); return;
        }
        runtime.memory().store32(out, id == 9u ? confirm_button : static_cast<std::uint32_t>(timezone_minutes));
        c.set_gpr(2, 0);
    });
}
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
