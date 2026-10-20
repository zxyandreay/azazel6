#include "psprecomp/elf32.hpp"
#include "psprecomp/runtime.hpp"
#include "psprecomp/sha256.hpp"
#include "psprecomp/common.hpp"
#include "probe_config.hpp"
#include "azazel6/bootstrap.hpp"
#include "azazel6/threads.hpp"
#include "azazel6/module.hpp"
#include "azazel6/settings.hpp"
#include "azazel6/graphics_memory.hpp"
#include "azazel6/ge_callbacks.hpp"
#include "azazel6/interrupts.hpp"
#include "azazel6/display.hpp"
#include <iostream>
#include <memory>
#include <cstdlib>

namespace psprecomp { void register_generated_functions(Runtime&); }

namespace azazel6 { void register_profile_imports(psprecomp::Runtime&); }

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
        register_generated_functions(*rt);
        azazel6::register_profile_imports(*rt);
        const auto bootstrap = azazel6::register_bootstrap_hle(*rt);
        rt->cpu().gpr[28] = module.gp;
        // Stack size/priority come from the validated module-start export.
        const auto stack_top = GuestMemory::kPhysicalBase + mem.size();
        constexpr auto bootstrap_size = (AZAZEL6_BOOTSTRAP_STACK_SIZE + 255u) & ~255u;
        for (const auto& segment : elf.segments()) {
            if (segment.type == 1 && static_cast<std::uint64_t>(segment.vaddr) + segment.memory_size > stack_top - bootstrap_size)
                throw Error("Experimental stack overlaps loaded segment");
        }
        rt->cpu().gpr[29] = stack_top - 16u;
        // This profile currently accepts exactly one loaded image; do not guess overlays.
        const ElfSegment* image_segment = nullptr;
        for (const auto& segment : elf.segments()) if (segment.type == 1) {
            if (image_segment) throw Error("Thread arena requires one loaded image");
            image_segment = &segment;
        }
        if (!image_segment || !(image_segment->flags & 1u)) throw Error("No executable image");
        const auto image_end = image_segment->vaddr + image_segment->memory_size;
        azazel6::register_module_hle(*rt, image_segment->vaddr, image_segment->memory_size);
        const auto allocator = std::make_shared<azazel6::GuestAllocator>(
            (image_end + 255u) & ~255u, stack_top - bootstrap_size);
        const auto partition = azazel6::register_memory_hle(*rt, allocator);
        const auto data_cache = azazel6::register_data_cache_hle(*rt,
            (image_segment->vaddr + image_segment->file_size + 63u) & ~63u, stack_top);
        const auto language = azazel6::register_language_hle(*rt,
            {AZAZEL6_LANGUAGE, AZAZEL6_CONFIRM_BUTTON},
            (image_segment->vaddr + image_segment->file_size + 3u) & ~3u, stack_top);
        const auto graphics_memory = azazel6::register_graphics_memory_hle(*rt);
        const auto ge_callbacks = azazel6::register_ge_callback_hle(*rt,
            image_segment->vaddr, image_segment->vaddr + image_segment->file_size);
        const auto interrupts = azazel6::register_interrupt_hle(*rt,
            image_segment->vaddr, image_segment->vaddr + image_segment->file_size, module.gp);
        const auto threads = azazel6::register_thread_hle(*rt, {
            (image_end + 255u) & ~255u, stack_top - bootstrap_size,
            image_segment->vaddr, image_end, image_segment->vaddr,
            image_segment->vaddr + image_segment->file_size, module.gp,
            AZAZEL6_BOOTSTRAP_PRIORITY, bootstrap_size}, allocator);
        const auto interrupt_delivery = azazel6::register_interrupt_delivery(*rt, interrupts, threads, allocator);
        // Bounded research mode: jump only while every thread is blocked.
        // Runnable CPU time is not modeled; these frames are not measured game timing.
        const auto display = azazel6::register_display_hle(*rt, threads, interrupt_delivery, 64);
        std::cout << "guest_clock_mode=bounded_idle_events frame_budget=64" << std::endl;
        std::cout << "entry=" << hex32(elf.runtime_entry()) << " gp=" << hex32(module.gp)
                  << " ram=" << mem.size() << " imports=" << imports.size()
                  << " initial_sp=" << hex32(rt->cpu().gpr[29]) << std::endl;
        if (!rt->has_function(elf.runtime_entry())) throw Error("Entry has no AOT registration");
        // Bounded startup dispatch; all unimplemented imports remain rejecting.
        set_runtime_pre_dispatch_hook([](Runtime& runtime, AllegrexContext& context,
                                         std::uint32_t pc, std::int32_t) {
            std::cout << "dispatch=" << hex32(pc) << " a0=" << hex32(context.gpr[4])
                      << " a1=" << hex32(context.gpr[5]) << " a2=" << hex32(context.gpr[6])
                      << " a3=" << hex32(context.gpr[7]) << " t0=" << hex32(context.gpr[8])
                      << " t1=" << hex32(context.gpr[9]) << std::endl;
            if (!runtime.has_function(pc)) runtime.stop("Uncovered guest address " + hex32(pc));
        });
        // Startup initializes 2048 records through six dispatch boundaries each.
        // Keep a finite diagnostic allowance; run_probe.py also enforces 15 seconds.
        rt->run(elf.runtime_entry(), 32768);
        std::cout << "after_pc=" << hex32(rt->cpu().pc) << " ra=" << hex32(rt->cpu().gpr[31])
                  << " sp=" << hex32(rt->cpu().gpr[29]) << " reason=" << rt->stop_reason() << std::endl;
        std::cout << "data_cache_ranges=" << data_cache->completed_ranges << std::endl;
        std::cout << "edram_size_reads=" << graphics_memory->size_reads
                  << " edram_address_reads=" << graphics_memory->address_reads << std::endl;
        std::cout << "ge_callback_count=" << ge_callbacks->count() << std::endl;
        std::cout << "vblank_registered=" << interrupts->vblank.has_value()
                  << " vblank_enabled=" << interrupts->enabled << std::endl;
        std::cout << "display_buffer_requests=" << display->buffer_requests
                  << " display_frames=" << display->clock.frames()
                  << " display_idle_frames=" << display->idle_frames()
                  << " display_latch_pending=" << display->buffers.pending() << std::endl;
        std::cout << "language=" << language->language << " confirm_button=" << language->confirm_button
                  << " language_reads=" << language->reads << " language_writes=" << language->writes << std::endl;
        std::cout << "callback_count=" << threads->callbacks().size()
                  << " exit_callback=" << hex32(threads->exit_callback()) << std::endl;
        if (rt->stop_reason().starts_with("Unverified indirect target ")) {
            const auto arg0 = rt->cpu().gpr[4];
            std::cout << "indirect_arg0=" << hex32(arg0) << " v0=" << hex32(rt->cpu().gpr[2])
                      << " v1=" << hex32(rt->cpu().gpr[3]) << " t9=" << hex32(rt->cpu().gpr[25]);
            if (!(arg0 & 3u) && arg0 >= GuestMemory::kPhysicalBase && static_cast<std::uint64_t>(arg0) + 4 <=
                static_cast<std::uint64_t>(GuestMemory::kPhysicalBase) + mem.size())
                std::cout << " arg0_word=" << hex32(mem.load32(arg0));
            std::cout << std::endl;
        }
        std::cout << "settings_completed=" << bootstrap->completed_calls
                  << " sdk_argument=" << hex32(bootstrap->last_sdk_argument)
                  << " game_info_flags=" << hex32(bootstrap->game_info_flags)
                  << " compiler_version=" << hex32(bootstrap->compiler_version) << std::endl;
        for (const auto& thread : threads->snapshot())
            std::cout << "thread=" << thread.uid << " status=" << static_cast<std::uint32_t>(thread.status)
                      << " entry=" << hex32(thread.entry)
                      << " priority=" << thread.priority << " stack_base=" << hex32(thread.stack_base)
                      << " stack_size=" << thread.stack_size << " gp=" << hex32(thread.gp) << std::endl;
        for (const auto& [uid, block] : partition->blocks)
            std::cout << "partition_block=" << uid << " base=" << hex32(block.base)
                      << " size=" << hex32(block.size) << std::endl;
        std::cout << "mutex_count=" << threads->mutexes().size() << std::endl;
        std::cout << "inactive_timer_count=" << threads->timers().size() << std::endl;
        std::cout << "fixed_pool_count=" << threads->fixed_pools().size() << std::endl;
        std::cout << "event_flag_count=" << threads->event_flags().size() << std::endl;
        for (const auto& [uid, pool] : threads->fixed_pools())
            std::cout << "fixed_pool=" << hex32(uid) << " base=" << hex32(pool.allocation.base)
                      << " block_size=" << pool.block_size << " allocated=" << pool.allocated << std::endl;
        return rt->stop_reason().starts_with("Missing HLE import ") ? 3 : 1;
    } catch (const std::exception& error) {
        std::cerr << "entry_probe: " << error.what() << '\n';
        return 1;
    }
}
