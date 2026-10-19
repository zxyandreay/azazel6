#include "azazel6/graphics_memory.hpp"
#include "psprecomp/common.hpp"

namespace azazel6 {
std::shared_ptr<GraphicsMemoryQueries> register_graphics_memory_hle(psprecomp::Runtime& rt) {
    using Memory = psprecomp::GuestMemory;
    const auto size = rt.memory().vram_size();
    if (size != Memory::kVramSize || !rt.memory().contains(Memory::kVramPhysicalBase, size))
        throw psprecomp::Error("Graphics queries require the pinned canonical EDRAM allocation");
    auto state = std::make_shared<GraphicsMemoryQueries>();
    rt.nids().add("sceGe_user", 0x1F6752ADu, "sceGeEdramGetSize");
    rt.nids().add("sceGe_user", 0xE47E40E4u, "sceGeEdramGetAddr");
    rt.register_hle("sceGe_user", 0x1F6752ADu, [state, size](auto&, auto& c) {
        ++state->size_reads; c.set_gpr(2, size);
    });
    rt.register_hle("sceGe_user", 0xE47E40E4u, [state](auto&, auto& c) {
        ++state->address_reads; c.set_gpr(2, Memory::kVramPhysicalBase);
    });
    return state;
}
}
