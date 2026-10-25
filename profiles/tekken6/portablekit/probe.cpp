#include "profile.hpp"
#include "system.hpp"
#include "kernel/kernel.hpp"
#include "psprecomp/common.hpp"
#include "psprecomp/sha256.hpp"
#include "azazel6/bootstrap.hpp"
#include "azazel6/clock_conversion.hpp"
#include <iostream>

int portablekit_ge_contracts();
int portablekit_decode_clear(const char*);

namespace {
struct Nid { const char* library; std::uint32_t nid; const char* name; };
constexpr Nid nids[] = {
#include "nid_table.inc"
};
}
namespace portablekit {
const GameProfile& game() {
    static const auto profile = [] {
        GameProfile p{};
        p.app_name = "azazel6_portablekit_probe"; p.project_name = "Azazel6 research";
        p.env_prefix = "AZAZEL6_EVAL"; p.data_organization = "Azazel6"; p.data_application = "Research";
        p.disc_id = "ULUS10466"; p.disc_id_display = "ULUS-10466"; p.game_title = "Tekken 6";
        p.executable_path_on_disc = "PSP_GAME/SYSDIR/EBOOT.BIN";
        p.param_sfo_path_on_disc = "PSP_GAME/PARAM.SFO";
        p.encrypted_executable_sha256 = ""; // Installer is never called; no keys are supplied.
        p.executable_sha256 = "e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f";
        p.load_base = 0; p.guest_ram_bytes = 32u * 1024u * 1024u;
        p.boot_path = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN";
        p.save_game_name = "ULUS10466"; p.adhoc_product_code = "ULUS10466";
        p.system_language = 1; p.confirm_button = 1;
        return p;
    }();
    return profile;
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--test-ge") return portablekit_ge_contracts();
        if ((argc != 3 && argc != 4) || psprecomp::sha256_file(argv[1]) != portablekit::game().executable_sha256)
            throw psprecomp::Error("Exact verified ELF and local output directory required");
        if (argc == 4) {
            if (std::string_view(argv[2]) != "--decode-clear") throw psprecomp::Error("Unknown evidence mode");
            return portablekit_decode_clear(argv[3]);
        }
        if (!portablekit::env_set("STRICT_HLE")) throw psprecomp::Error("Strict HLE required");
        const auto elf = psprecomp::Elf32Image::from_file(argv[1]);
        psprecomp::Runtime rt(portablekit::game().guest_ram_bytes);
        for (const auto& n : nids) rt.nids().add(n.library, n.nid, n.name);
        (void)elf.load_and_relocate(rt.memory(), 0);
        const auto module = elf.find_module_info(rt.memory(), 0);
        if (elf.runtime_entry(0) != 0x08804124u || !module || module->gp != 0x08C022B0u ||
            module->name != "tekken" || elf.scan_imports(rt.memory(), *module).size() != 268)
            throw psprecomp::Error("Unexpected executable/module/import identity");
        std::cout << "Exact input admitted; headless diagnostic interpretation, not AOT boot verification\n";
        portablekit::install_system(rt, elf, {{}, std::filesystem::path(argv[2]) / "ms0"});
        const auto bootstrap = azazel6::register_bootstrap_hle(rt);
        // Reuse the same checked conversion, with this kernel's stack owner.
        rt.register_hle("ThreadManForUser", 0xE1619D7Cu, [](auto& runtime, auto& c) {
            const auto* current = portablekit::kernel().current_thread();
            if (!current || current->status != portablekit::ThreadStatus::Running || portablekit::kernel().in_interrupt()) {
                runtime.stop("Unsupported clock-conversion context"); return;
            }
            azazel6::convert_sysclock_wide(runtime, c, current->stack_bottom, current->stack_size);
        });
        rt.run(elf.runtime_entry(0), 100000);
        std::cout << "stop=" << rt.stop_reason() << " pc=" << psprecomp::hex32(rt.cpu().pc)
                  << " metadata_calls=" << bootstrap->completed_calls << '\n';
        rt.report_hle_histogram();
        std::cout << portablekit::kernel().describe_threads();
        return 3;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
