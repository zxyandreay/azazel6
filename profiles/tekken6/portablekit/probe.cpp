#include "profile.hpp"
#include "system.hpp"
#include "kernel/kernel.hpp"
#include "psprecomp/common.hpp"
#include "psprecomp/sha256.hpp"
#include "psprecomp/interpreter.hpp"
#include "psprecomp/runtime.hpp"
#include "azazel6/bootstrap.hpp"
#include "azazel6/clock_conversion.hpp"
#include "azazel6/media.hpp"
#include "cancel_wakeup.hpp"
#include "hle/hle_common.hpp"
#include <bit>
#include <iostream>
#ifdef PORTABLEKIT_HAS_RENDERER
#include "gpu/vulkan_renderer.hpp"
#endif

int portablekit_ge_contracts();
int portablekit_decode_clear(const char*);
int portablekit_renderer_contracts();

#ifdef AZAZEL6_ENTRY_AOT
namespace psprecomp {
void Tekken6Entry(CorpusRuntime&, AllegrexContext&);
}
#endif

namespace {
std::uint64_t native_aot_entry_calls = 0;
#ifdef AZAZEL6_ENTRY_AOT
void counted_entry(psprecomp::CorpusRuntime& runtime, psprecomp::AllegrexContext& ctx) {
    ++native_aot_entry_calls;
    psprecomp::Tekken6Entry(runtime, ctx);
}
#endif
struct Nid { const char* library; std::uint32_t nid; const char* name; };
constexpr Nid nids[] = {
#include "nid_table.inc"
};
void register_research_hle(portablekit::HleRegistrar& hle) {
    hle.add("ThreadManForUser", "sceKernelCancelWakeupThread", [](auto& runtime, auto& c) {
        auto& kernel = portablekit::kernel();
        const auto* current = kernel.current_thread();
        if (!current || current->status != portablekit::ThreadStatus::Running ||
            !kernel.interrupts_enabled() || kernel.in_interrupt()) {
            runtime.stop("Cancel wakeup requires a running interruptible thread");
            return;
        }
        const auto uid = c.gpr[4] ? std::bit_cast<portablekit::SceUID>(c.gpr[4]) : kernel.current_uid();
        kernel.finish(c, azazel6::cancel_wakeup(kernel.find_thread(uid)));
    });
}
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
        p.register_extra_hle = register_research_hle;
        return p;
    }();
    return profile;
}
}
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--test-ge") return portablekit_ge_contracts();
        if (argc == 2 && std::string_view(argv[1]) == "--test-renderer") return portablekit_renderer_contracts();
        const bool capture_game = argc == 11 && std::string_view(argv[7]) == "--capture-game-frame" &&
            std::string_view(argv[9]) == "--dispatches" &&
            (std::string_view(argv[10]) == "100000" || std::string_view(argv[10]) == "250000");
        const bool media_run = (argc == 7 || capture_game) && std::string_view(argv[3]) == "--iso" &&
            std::string_view(argv[5]) == "--media-gate" && std::string_view(argv[6]) == "stdin";
        if ((argc != 3 && argc != 4 && !media_run) ||
            psprecomp::sha256_file(argv[1]) != portablekit::game().executable_sha256)
            throw psprecomp::Error("Exact verified ELF and local output directory required");
        if (argc == 4) {
            if (std::string_view(argv[2]) != "--decode-clear") throw psprecomp::Error("Unknown evidence mode");
            return portablekit_decode_clear(argv[3]);
        }
        if (!portablekit::env_set("STRICT_HLE")) throw psprecomp::Error("Strict HLE required");
        std::filesystem::path disc_image;
        if (media_run) {
            constexpr std::string_view media_sha = "45e9eaa105afda7c44204909e7fa22321236c89158bd7d26b52aba2f2e0794c8";
            azazel6::IsoMedia checked(argv[4], std::string(media_sha));
            if (checked.size() != 855638016u) throw psprecomp::Error("Unsupported media size");
            std::cout << "media_sha256=" << checked.fingerprint() << " media_bytes=" << checked.size()
                      << " media_entries=" << checked.file_count() << '\n';
            std::cout << "validated_media_waiting" << std::endl;
            if (std::cin.get() != 'G') throw psprecomp::Error("Missing runner execution grant");
            disc_image = argv[4];
        }
        const auto elf = psprecomp::Elf32Image::from_file(argv[1]);
        psprecomp::Runtime rt(portablekit::game().guest_ram_bytes);
        for (const auto& n : nids) rt.nids().add(n.library, n.nid, n.name);
        rt.nids().add("ThreadManForUser", 0xFCCFAD26u, "sceKernelCancelWakeupThread");
        // Pinned host/hle/hle_psmfplayer.cpp already supplies this checked CB path.
        rt.nids().add("scePsmfPlayer", 0x58B83577u, "scePsmfPlayerSetPsmfCB");
        (void)elf.load_and_relocate(rt.memory(), 0);
        const auto module = elf.find_module_info(rt.memory(), 0);
        if (elf.runtime_entry(0) != 0x08804124u || !module || module->gp != 0x08C022B0u ||
            module->name != "tekken" || elf.scan_imports(rt.memory(), *module).size() != 268)
            throw psprecomp::Error("Unexpected executable/module/import identity");
        psprecomp::register_generated_functions(rt);
#ifdef AZAZEL6_ENTRY_AOT
        if (!rt.has_function(elf.runtime_entry(0)))
            throw psprecomp::Error("Linked corpus omitted the reviewed Tekken entry");
        rt.register_function(elf.runtime_entry(0), counted_entry, "Tekken6Entry/counted");
#endif
        const bool entry_aot_linked = rt.has_function(elf.runtime_entry(0));
        std::cout << "Exact input admitted; " << (capture_game ? "Vulkan" : "headless")
                  << " hybrid diagnostic entry_aot_linked=" << entry_aot_linked << '\n';
        portablekit::install_system(rt, elf, {disc_image, std::filesystem::path(argv[2]) / "ms0"});
#ifdef PORTABLEKIT_HAS_RENDERER
        if (capture_game && !portablekit::active_renderer())
            throw psprecomp::Error("Explicit game rendering requires an available Vulkan renderer");
#else
        if (capture_game) throw psprecomp::Error("Game rendering requires an explicit Vulkan build");
#endif
        const auto bootstrap = azazel6::register_bootstrap_hle(rt);
        // Reuse the same checked conversion, with this kernel's stack owner.
        rt.register_hle("ThreadManForUser", 0xE1619D7Cu, [](auto& runtime, auto& c) {
            const auto* current = portablekit::kernel().current_thread();
            if (!current || current->status != portablekit::ThreadStatus::Running || portablekit::kernel().in_interrupt()) {
                runtime.stop("Unsupported clock-conversion context"); return;
            }
            azazel6::convert_sysclock_wide(runtime, c, current->stack_bottom, current->stack_size);
        });
        const auto dispatch_limit = capture_game && std::string_view(argv[10]) == "250000" ? 250000u : 100000u;
        std::cout << "dispatch_limit=" << dispatch_limit << '\n';
        rt.run(elf.runtime_entry(0), dispatch_limit);
        const auto interpreted = psprecomp::interpreter_stats();
        std::cout << "native_aot_entry_calls=" << native_aot_entry_calls
                  << " interpreter_instructions=" << interpreted.instructions << '\n';
#ifdef PORTABLEKIT_HAS_RENDERER
        if (capture_game) {
            if (!portablekit::active_renderer()->capture_frame(argv[8]))
                throw psprecomp::Error("No capturable game target at bounded stop");
            std::cout << "game_frame_captured" << '\n';
        }
#endif
        std::cout << "stop=" << rt.stop_reason() << " pc=" << psprecomp::hex32(rt.cpu().pc)
                  << " metadata_calls=" << bootstrap->completed_calls << '\n';
        if (rt.stop_reason().starts_with("Missing HLE import ")) {
            const auto& c = rt.cpu();
            std::cout << "[missing-hle] uid=" << portablekit::kernel().current_uid()
                      << " a0=" << psprecomp::hex32(c.gpr[4]) << " a1=" << psprecomp::hex32(c.gpr[5])
                      << " a2=" << psprecomp::hex32(c.gpr[6]) << " a3=" << psprecomp::hex32(c.gpr[7])
                      << " ra=" << psprecomp::hex32(c.gpr[31]) << '\n';
        }
        rt.report_hle_histogram();
        std::cout << portablekit::kernel().describe_threads();
        return 3;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
