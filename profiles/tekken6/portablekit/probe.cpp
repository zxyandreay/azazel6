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
#include "install/executable_preparation.hpp"
#include "install/user_data.hpp"
#include <bit>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
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
constexpr std::string_view media_sha = "45e9eaa105afda7c44204909e7fa22321236c89158bd7d26b52aba2f2e0794c8";
constexpr std::string_view pinned_elf_sha = "e0ed418952c51032796ebc54584592a9f6075495229f6d12d5065f3186bcb55f";
constexpr std::string_view prepared_elf_sha = "60eb876806e8625161c24e65aab6d906fc9d5740777fbcfafe7ef0fa9920c01d";
std::uint64_t native_aot_entry_calls = 0;
#ifdef AZAZEL6_ENTRY_AOT
void counted_entry(psprecomp::CorpusRuntime& runtime, psprecomp::AllegrexContext& ctx) {
    ++native_aot_entry_calls;
#ifdef __ANDROID__
    std::cout << "native_aot_entry_calls=" << native_aot_entry_calls << '\n';
#endif
    psprecomp::Tekken6Entry(runtime, ctx);
#ifdef __ANDROID__
    std::cout << "interpreter_instructions_after_aot=" << psprecomp::interpreter_stats().instructions << '\n';
#endif
}
#endif
std::shared_ptr<azazel6::BootstrapState> register_checked_bootstrap(psprecomp::Runtime& runtime) {
    auto bootstrap = azazel6::register_bootstrap_hle(runtime);
    runtime.register_hle("ThreadManForUser", 0xE1619D7Cu, [](auto& rt, auto& c) {
        const auto* current = portablekit::kernel().current_thread();
        if (!current || current->status != portablekit::ThreadStatus::Running || portablekit::kernel().in_interrupt()) {
            rt.stop("Unsupported clock-conversion context"); return;
        }
        azazel6::convert_sysclock_wide(rt, c, current->stack_bottom, current->stack_size);
    });
    return bootstrap;
}
#ifdef __ANDROID__
void validate_android_image(psprecomp::Runtime& runtime, const psprecomp::Elf32Image& elf) {
    const auto installed = portablekit::install::find_installation(portablekit::install::user_data_directory());
    if (!installed || psprecomp::sha256_file(installed->executable) != prepared_elf_sha)
        throw psprecomp::Error("Android installation lacks the exact prepared Tekken executable");
    azazel6::IsoMedia checked(installed->disc_image, std::string(media_sha));
    if (checked.size() != 855638016u) throw psprecomp::Error("Unsupported Tekken media size");
    const auto entry = checked.find("disc0:/PSP_GAME/SYSDIR/EBOOT.BIN");
    if (!entry || entry->directory || entry->size != 4158480u)
        throw psprecomp::Error("Unexpected encrypted Tekken EBOOT extent");
    const auto encrypted = checked.read(*entry, 0, entry->size);
    auto prepared = portablekit::install::prepare_executable(encrypted);
    if (prepared.size() != 4158144u || psprecomp::sha256_bytes(prepared) != prepared_elf_sha)
        throw psprecomp::Error("Prepared Tekken executable changed");
    // The previously verified AOT source is this payload followed by the 0x150
    // encrypted trailer. No loaded instruction differs; pin that relation too.
    prepared.insert(prepared.end(), encrypted.end() - 0x150, encrypted.end());
    if (psprecomp::sha256_bytes(prepared) != pinned_elf_sha)
        throw psprecomp::Error("Prepared executable does not match the AOT source");
    const auto module = elf.find_module_info(runtime.memory(), 0);
    if (elf.runtime_entry(0) != 0x08804124u || !module || module->gp != 0x08C022B0u ||
        module->name != "tekken" || elf.scan_imports(runtime.memory(), *module).size() != 268)
        throw psprecomp::Error("Unexpected Tekken executable/module/import identity");
#ifdef AZAZEL6_ENTRY_AOT
    if (!runtime.has_function(elf.runtime_entry(0)))
        throw psprecomp::Error("Android hybrid build lacks the native Tekken entry");
    runtime.register_function(elf.runtime_entry(0), counted_entry, "Tekken6Entry/counted");
#else
    throw psprecomp::Error("Android hybrid build lacks an AOT corpus");
#endif
    (void)register_checked_bootstrap(runtime);
    std::cout << "exact_iso_sha256=" << checked.fingerprint()
              << " prepared_elf_sha256=" << prepared_elf_sha
              << " aot_source_sha256=" << pinned_elf_sha << '\n';
}
#endif
struct Nid { const char* library; std::uint32_t nid; const char* name; };
constexpr Nid nids[] = {
#include "nid_table.inc"
};
void register_research_hle(portablekit::HleRegistrar& hle) {
    hle.bind("ThreadManForUser", 0xFCCFAD26u, [](auto& runtime, auto& c) {
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
    // Pinned PortableKit uses this same checked SetPsmf handler for the CB
    // variant when its NID is known. Reuse its function and player state.
    if (auto set = hle.bound_function("scePsmfPlayer", 0x3D6D25A9u))
        hle.bind("scePsmfPlayer", 0x58B83577u, std::move(set));
}
}
namespace portablekit {
const GameProfile& game() {
    static const auto profile = [] {
#ifdef __ANDROID__
        // The Android app has no shell environment. Strict import refusal is mandatory.
        setenv("AZAZEL6_EVAL_STRICT_HLE", "1", 1);
        // First-device diagnostics must stop even when the guest is still runnable.
        setenv("PSPRECOMP_MAX_DISPATCHES", "100000", 0);
#endif
        GameProfile p{};
        p.app_name = "azazel6_portablekit_probe"; p.project_name = "Azazel6 research";
        p.env_prefix = "AZAZEL6_EVAL"; p.data_organization = "Azazel6"; p.data_application = "Research";
        p.disc_id = "ULUS10466"; p.disc_id_display = "ULUS-10466"; p.game_title = "Tekken 6";
        p.executable_path_on_disc = "PSP_GAME/SYSDIR/EBOOT.BIN";
        p.param_sfo_path_on_disc = "PSP_GAME/PARAM.SFO";
        p.encrypted_executable_sha256 = "254af6ab3359d290ad3ed47abf036de9aae239b1695d6c3d504bb47ef94ed6a7";
#ifdef __ANDROID__
        p.executable_sha256 = prepared_elf_sha.data();
#else
        p.executable_sha256 = pinned_elf_sha.data();
#endif
        // Published PSP tag material: PPSSPP Core/ELF/PrxDecrypter.cpp, key_D91612F0.
        // The installer still checks both input and decrypted-output SHA-256 values.
        p.decryption_tag = 0xD91612F0u;
        p.decryption_key = {0x9E, 0x20, 0xE1, 0xCD, 0xD7, 0x88, 0xDE, 0xC0,
                            0x31, 0x9B, 0x10, 0xAF, 0xC5, 0xB8, 0x73, 0x23};
        p.load_base = 0; p.guest_ram_bytes = 32u * 1024u * 1024u;
        p.boot_path = "disc0:/PSP_GAME/SYSDIR/EBOOT.BIN";
        p.save_game_name = "ULUS10466"; p.adhoc_product_code = "ULUS10466";
        p.system_language = 1; p.confirm_button = 1;
        p.register_extra_hle = register_research_hle;
#ifdef __ANDROID__
        p.patch_loaded_image = validate_android_image;
#endif
        return p;
    }();
    return profile;
}
}
#ifndef __ANDROID__
int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--test-ge") return portablekit_ge_contracts();
        if (argc == 2 && std::string_view(argv[1]) == "--test-renderer") return portablekit_renderer_contracts();
        if (argc == 3 && std::string_view(argv[1]) == "--check-disc-executable") {
            azazel6::IsoMedia checked(argv[2], std::string(media_sha));
            if (checked.size() != 855638016u) throw psprecomp::Error("Unsupported media size");
            const auto entry = checked.find("disc0:/PSP_GAME/SYSDIR/EBOOT.BIN");
            if (!entry || entry->directory || entry->size != 4158480u)
                throw psprecomp::Error("Unexpected encrypted EBOOT extent");
            const auto encrypted = checked.read(*entry, 0, entry->size);
            const auto& profile = portablekit::game();
            const portablekit::install::TagKeyMaterial tag{profile.decryption_tag, profile.decryption_key,
                                                           profile.decryption_key_table};
            const auto raw = portablekit::install::decrypt_executable(encrypted, tag);
            if (raw.size() != 4158144u || psprecomp::sha256_bytes(raw) != prepared_elf_sha)
                throw psprecomp::Error("Unexpected prepared ELF payload");
            auto reconstructed = raw;
            reconstructed.insert(reconstructed.end(), encrypted.end() - 0x150, encrypted.end());
            if (psprecomp::sha256_bytes(reconstructed) != pinned_elf_sha)
                throw psprecomp::Error("Prepared payload/trailer does not match the pinned AOT ELF");
            std::cout << "encrypted_eboot_sha256=" << psprecomp::sha256_bytes(encrypted)
                      << " prepared_elf_sha256=" << psprecomp::sha256_bytes(raw)
                      << " pinned_aot_elf_sha256=" << psprecomp::sha256_bytes(reconstructed) << '\n';
            return 0;
        }
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
        const auto bootstrap = register_checked_bootstrap(rt);
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
#endif
