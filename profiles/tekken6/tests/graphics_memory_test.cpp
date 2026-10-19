#include "azazel6/graphics_memory.hpp"
#include "azazel6/bootstrap.hpp"
#include <iostream>
#include <stdexcept>

namespace {
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
}
int main() {
    try {
        using Memory = psprecomp::GuestMemory;
        auto rt = std::make_unique<psprecomp::Runtime>();
        const auto state = azazel6::register_graphics_memory_hle(*rt);
        const auto base = Memory::kVramPhysicalBase, size = rt->memory().vram_size();
        rt->memory().store32(base, 0x12345678u);
        rt->memory().store32(base + size - 4, 0xAABBCCDDu);
        rt->memory().store32(Memory::kPhysicalBase, 0xFFEEDDCCu);
        for (auto nid : {0x1F6752ADu, 0xE47E40E4u, 0x1F6752ADu}) {
            auto& c = rt->cpu();
            for (unsigned i = 1; i < 32; ++i) c.gpr[i] = 0x12340000u + i;
            c.pc = 0x08801000u; c.gpr[31] = 0x08802000u;
            c.hi = 71; c.lo = 93;
            auto expected = c.gpr;
            expected[2] = nid == 0x1F6752ADu ? size : base;
            azazel6::invoke_returning_import(*rt, c, "sceGe_user", nid);
            check(!rt->stopped() && c.gpr == expected && c.pc == 0x08802000u && c.hi == 71 && c.lo == 93,
                  "Query changed unrelated CPU state or returned a host pointer");
            check(rt->memory().load32(base) == 0x12345678u &&
                  rt->memory().load32(base + size - 4) == 0xAABBCCDDu &&
                  rt->memory().load32(Memory::kPhysicalBase) == 0xFFEEDDCCu,
                  "Geometry query changed RAM or EDRAM");
        }
        check(base == 0x04000000u && size == 0x200000u && rt->memory().contains(base, size),
              "Query geometry does not match the pinned PSP memory model");
        check(state->size_reads == 2 && state->address_reads == 1, "Query accounting");
        auto other = std::make_unique<psprecomp::Runtime>();
        const auto isolated = azazel6::register_graphics_memory_hle(*other);
        check(isolated->size_reads == 0 && isolated->address_reads == 0 &&
              other->memory().load32(base) == 0, "Graphics memory state leaked between runtimes");
        other->cpu().gpr[2] = 0xDEADBEEFu;
        other->invoke_import("sceGe_user", 0xFFFFFFFFu, other->cpu());
        check(other->stopped() && other->cpu().gpr[2] == 0xDEADBEEFu, "Unimplemented GE service accepted");
        std::cout << "Graphics memory query contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
