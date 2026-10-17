#include "psprecomp/elf32.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/sha256.hpp"
#include "psprecomp/common.hpp"
#include "probe_config.hpp"
#include <iostream>
#include <memory>
#include <unordered_map>
#include <cstdlib>

namespace psprecomp { void register_generated_functions(Runtime&); }

namespace {
std::unordered_map<std::uint32_t, psprecomp::PspImport> import_stubs;
bool import_reached = false;
void import_boundary(psprecomp::Runtime& runtime, psprecomp::AllegrexContext& context) {
    const auto& import = import_stubs.at(context.pc);
    import_reached = true;
    runtime.invoke_import(import.library, import.nid, context);
}
}

int main(int argc, char** argv) {
    using namespace psprecomp;
    if (argc != 2) { std::cerr << "Usage: azazel6_entry_probe <validated EBOOT>\n"; return 2; }
    try {
#ifdef _WIN32
        if (_putenv_s("PSPRECOMP_NO_INTERPRETER", "1") != 0) throw Error("Cannot disable interpreter");
#else
        if (setenv("PSPRECOMP_NO_INTERPRETER", "1", 1) != 0) throw Error("Cannot disable interpreter");
#endif
        if (sha256_file(argv[1]) != AZAZEL6_EXPECTED_SHA) throw Error("Unsupported input SHA-256");
        const auto elf = Elf32Image::from_file(argv[1]);
        auto rt = std::make_unique<Runtime>(elf.required_ram_size());
        const auto relocations = elf.load_and_relocate(rt->memory());
        if (relocations.invalid || relocations.unsupported) throw Error("Unsupported relocation");
        const auto address = AZAZEL6_MODULE_ADDRESS;
        auto& mem = rt->memory();
        if (!mem.contains(address, 52)) throw Error("Module metadata outside guest memory");
        PspModuleInfo module{mem.load16(address), mem.load8(address + 2), mem.load8(address + 3),
            mem.read_c_string(address + 4, 28), mem.load32(address + 32),
            mem.load32(address + 36), mem.load32(address + 40), mem.load32(address + 44),
            mem.load32(address + 48), address};
        const auto imports = elf.scan_imports(mem, module);
        for (const auto& import : imports) {
            import_stubs.emplace(import.stub_address, import);
            rt->register_function(import.stub_address, &import_boundary, "import_boundary");
            rt->register_hle(import.library, import.nid,
                [import](Runtime& runtime, AllegrexContext&) {
                    runtime.stop("Unimplemented PSP import " + import.library + ":" + hex32(import.nid));
                });
        }
        register_generated_functions(*rt);
        rt->cpu().gpr[28] = module.gp;
        // Experimental host-owned stack reservation at top of ELF-sized RAM.
        // It is not a discovered PSP thread stack requirement.
        const auto stack_top = GuestMemory::kPhysicalBase + mem.size();
        for (const auto& segment : elf.segments()) {
            if (segment.type == 1 && static_cast<std::uint64_t>(segment.vaddr) + segment.memory_size > stack_top - 65536u)
                throw Error("Experimental stack overlaps loaded segment");
        }
        rt->cpu().gpr[29] = stack_top - 16u;
        std::cout << "entry=" << hex32(elf.runtime_entry()) << " gp=" << hex32(module.gp)
                  << " ram=" << mem.size() << " imports=" << imports.size()
                  << " initial_sp=" << hex32(rt->cpu().gpr[29]) << std::endl;
        if (!rt->has_function(elf.runtime_entry())) throw Error("Entry has no AOT registration");
        // AOT entry then a registered import trap. Stop before any uncovered code.
        set_runtime_pre_dispatch_hook([](Runtime& runtime, AllegrexContext& context,
                                         std::uint32_t pc, std::int32_t) {
            std::cout << "dispatch=" << hex32(pc) << " a0=" << hex32(context.gpr[4]) << std::endl;
            if (!runtime.has_function(pc)) runtime.stop("Uncovered guest address " + hex32(pc));
        });
        rt->run(elf.runtime_entry(), 2);
        std::cout << "after_pc=" << hex32(rt->cpu().pc) << " ra=" << hex32(rt->cpu().gpr[31])
                  << " sp=" << hex32(rt->cpu().gpr[29]) << " reason=" << rt->stop_reason() << std::endl;
        return import_reached ? 3 : 1; // Never report an unexpected stop as the expected boundary.
    } catch (const std::exception& error) {
        std::cerr << "entry_probe: " << error.what() << '\n';
        return 1;
    }
}
