#include "azazel6/module.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>

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
    rt.nids().add("ModuleMgrForUser", 0x977DE386u, "sceKernelLoadModule");
    rt.register_hle("ModuleMgrForUser", 0x977DE386u, [](auto& runtime, auto& c) {
        if (c.gpr[5] || c.gpr[6]) {
            runtime.stop("Unsupported module-load flags or options"); return;
        }
        try {
            const auto path = runtime.memory().read_c_string(c.gpr[4], 256);
            // User-mode LoadModule rejects the Memory Stick device before loading.
            // The game explicitly handles SCE_KERNEL_ERROR_PROHIBIT_LOADMODULE_DEVICE.
            const auto name = path.starts_with("ms0:/") ? path.substr(5) : std::string{};
            if (!name.empty() && name.size() <= 64 &&
                    std::all_of(name.begin(), name.end(), [](unsigned char ch) {
                        return (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                            ch == '_' || ch == '.' || ch == '-';
                    })) {
                c.set_gpr(2, 0x80020146u); return;
            }
            runtime.stop("Unsupported module load path: " + path);
        } catch (const std::exception& error) { runtime.stop(error.what()); }
    });
}
}
