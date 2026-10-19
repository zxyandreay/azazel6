#include "azazel6/ge_callbacks.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>

namespace azazel6 {
std::size_t GeCallbacks::count() const noexcept {
    return std::count_if(slots.begin(), slots.end(), [](const auto& slot) { return slot.has_value(); });
}
std::shared_ptr<GeCallbacks> register_ge_callback_hle(psprecomp::Runtime& rt,
                                                    std::uint32_t begin, std::uint32_t end) {
    const auto ram_end = psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(rt.memory().size());
    if (begin < psprecomp::GuestMemory::kPhysicalBase || begin >= end || ((begin | end) & 3u) || end > ram_end)
        throw psprecomp::Error("Invalid GE callback executable range");
    auto state = std::make_shared<GeCallbacks>();
    rt.nids().add("sceGe_user", 0xA4FC06A4u, "sceGeSetCallback");
    rt.nids().add("sceGe_user", 0x05DB22CEu, "sceGeUnsetCallback");
    rt.register_hle("sceGe_user", 0xA4FC06A4u, [state, begin, end, ram_end](auto& runtime, auto& c) {
        const auto address = c.gpr[4];
        if ((address & 3u) || address < psprecomp::GuestMemory::kPhysicalBase ||
                static_cast<std::uint64_t>(address) + 16 > ram_end) {
            runtime.stop("GE callback record outside canonical RAM"); return;
        }
        const auto& memory = runtime.memory();
        const GeCallback callback{memory.load32(address), memory.load32(address + 4),
                                  memory.load32(address + 8), memory.load32(address + 12)};
        for (auto function : {callback.signal_function, callback.finish_function}) {
            if (function && ((function & 3u) || function < begin ||
                    static_cast<std::uint64_t>(function) + 4 > end)) {
                runtime.stop("GE callback function outside verified executable range"); return;
            }
        }
        const auto free = std::find_if(state->slots.begin(), state->slots.end(),
                                       [](const auto& slot) { return !slot.has_value(); });
        if (free == state->slots.end()) { c.set_gpr(2, 0x80000022u); return; }
        *free = callback; // Copy before returning: the original record may be on a guest stack.
        c.set_gpr(2, static_cast<std::uint32_t>(free - state->slots.begin()));
    });
    rt.register_hle("sceGe_user", 0x05DB22CEu, [state](auto&, auto& c) {
        const auto id = c.gpr[4];
        if (id >= state->slots.size()) { c.set_gpr(2, 0x80000100u); return; }
        state->slots[id].reset(); // Firmware accepts an already empty in-range slot.
        c.set_gpr(2, 0);
    });
    return state;
}
}
