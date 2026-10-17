#include "azazel6/module.hpp"
#include "psprecomp/common.hpp"

namespace azazel6 {
void register_module_hle(psprecomp::Runtime& rt, std::uint32_t begin, std::uint32_t size) {
    const auto end = static_cast<std::uint64_t>(begin) + size;
    if (!size || begin < 0x08000000u || end > 0x08000000ull + rt.memory().size())
        throw psprecomp::Error("Resident module outside canonical RAM");
    rt.register_hle("ModuleMgrForUser", 0xD8B73127u, [begin, end](auto&, auto& c) {
        const auto address = c.gpr[4];
        // This profile has user threads only; kernel aliases are inaccessible.
        c.set_gpr(2, address & 0x80000000u ? 0x800200D3u :
            address >= begin && address < end ? kMainModuleUid : 0x8002012Eu);
    });
}
}
