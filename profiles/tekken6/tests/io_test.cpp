#include "azazel6/bootstrap.hpp"
#include "azazel6/io.hpp"
#include <iostream>
#include <stdexcept>

using psprecomp::Runtime;
using namespace azazel6;
void require(bool condition, const char* message) { if (!condition) throw std::runtime_error(message); }

struct Fixture {
    std::unique_ptr<Runtime> rt = std::make_unique<Runtime>();
    std::shared_ptr<ThreadRegistry> threads;
    std::shared_ptr<IoStartupState> io;
    Fixture() {
        threads = register_thread_hle(*rt, {0x09000000u, 0x09010000u, 0x08804000u,
            0x08C00000u, 0x08804000u, 0x08C00000u, 0x08805000u, 40u, 0x4000u});
        io = register_io_startup_hle(*rt, threads);
        auto& c = rt->cpu();
        rt->memory().store8(0x08810000u, 'c'); rt->memory().store8(0x08810001u, 'b');
        rt->memory().store8(0x08810002u, 0);
        c.gpr[4] = 0x08810000u; c.gpr[5] = 0x08820000u; c.gpr[6] = 0;
        rt->invoke_import("ThreadManForUser", 0xE81CAF8Fu, c);
        require(c.gpr[2] == 0x50000000u, "Callback setup");
        const char name[] = "fatms0:";
        for (std::size_t i = 0; i < sizeof(name); ++i) rt->memory().store8(0x08811000u + i, name[i]);
        rt->memory().store32(c.gpr[29], c.gpr[2]);
        c.gpr[4] = 0x08811000u; c.gpr[5] = kFatmsRegisterCallbackCommand;
        c.gpr[6] = c.gpr[29]; c.gpr[7] = 4; c.gpr[8] = 0; c.gpr[9] = 0;
        c.pc = 0x08804000u; c.gpr[31] = 0x08805000u;
    }
};

void registration_contract() {
    Fixture f; auto& c = f.rt->cpu();
    invoke_returning_import(*f.rt, c, "IoFileMgrForUser", kIoDevctlNid);
    require(!f.rt->stopped() && c.gpr[2] == 0 && c.pc == 0x08805000u &&
            f.io->fatms_callbacks == std::vector<std::uint32_t>{0x50000000u},
            "fatms callback registration");
    c.pc = 0x08804000u; f.rt->invoke_import("IoFileMgrForUser", kIoDevctlNid, c);
    require(c.gpr[2] == 0x80010016u && f.io->fatms_callbacks.size() == 1,
            "Duplicate fatms callback accepted");
}

void rejection_contracts() {
    for (int failure = 0; failure < 4; ++failure) {
        Fixture f; auto& c = f.rt->cpu();
        if (failure == 0) c.gpr[5] = 0x02415822u;
        if (failure == 1) c.gpr[7] = 8;
        if (failure == 2) c.gpr[6] = 0x08812000u;
        if (failure == 3) f.rt->memory().store8(c.gpr[4], 'm');
        f.rt->invoke_import("IoFileMgrForUser", kIoDevctlNid, c);
        require(f.rt->stopped() && f.io->fatms_callbacks.empty(), "Invalid devctl mutated state");
    }
}

void wlan_address_contract() {
    Fixture f; auto& c = f.rt->cpu();
    const auto output = c.gpr[29] + 8;
    for (unsigned i = 0; i < 6; ++i) f.rt->memory().store8(output + i, 0xCC);
    c.gpr[4] = output; c.pc = 0x08804000u; c.gpr[31] = 0x08805000u;
    invoke_returning_import(*f.rt, c, "sceWlanDrv", kWlanGetEtherAddrNid);
    require(!f.rt->stopped() && c.gpr[2] == 0 && c.pc == 0x08805000u &&
            f.io->wlan_address_reads == 1, "WLAN address return");
    for (unsigned i = 0; i < 6; ++i)
        require(f.rt->memory().load8(output + i) == f.io->wlan_address[i], "WLAN address bytes");

    const auto current = f.threads->current_thread();
    require(current.has_value(), "WLAN current-thread fixture");
    for (const auto bad : {0u, current->stack_base - 1,
                           current->stack_base + current->stack_size - 5, 0xFFFFFFFFu}) {
        Fixture rejected; auto& rc = rejected.rt->cpu(); rc.gpr[4] = bad;
        rejected.rt->invoke_import("sceWlanDrv", kWlanGetEtherAddrNid, rc);
        require(rejected.rt->stopped() && rejected.io->wlan_address_reads == 0,
                "Invalid WLAN output mutated state");
    }
}

void umd_activation_contract() {
    Fixture f; auto& c = f.rt->cpu();
    constexpr std::uint32_t drive = 0x08004040u;
    const char name[] = "disc0:";
    for (unsigned i = 0; i < sizeof(name); ++i) f.rt->memory().store8(drive + i, name[i]);
    c.gpr[4] = 1; c.gpr[5] = drive;
    f.rt->invoke_import("sceUmdUser", kUmdActivateNid, c);
    require(!f.rt->stopped() && c.gpr[2] == 0 && f.io->umd_activated &&
            f.io->umd_activation_count == 1, "UMD activation return");

    for (const auto [mode, pointer] : {std::pair{0u, drive}, {3u, drive},
                                      {1u, 0u}, {1u, 0x09FFFFFFu}}) {
        Fixture rejected; auto& rc = rejected.rt->cpu(); rc.gpr[4] = mode; rc.gpr[5] = pointer;
        rejected.rt->invoke_import("sceUmdUser", kUmdActivateNid, rc);
        require(!rejected.rt->stopped() && rc.gpr[2] == 0x80010016u &&
                !rejected.io->umd_activated && rejected.io->umd_activation_count == 0,
                "Invalid UMD activation mutated state");
    }
    Fixture mode_two; mode_two.rt->cpu().gpr[4] = 2; mode_two.rt->cpu().gpr[5] = drive;
    mode_two.rt->invoke_import("sceUmdUser", kUmdActivateNid, mode_two.rt->cpu());
    require(mode_two.rt->stopped() && !mode_two.io->umd_activated,
            "Unsupported UMD activation mode mutated state");
    Fixture wrong; wrong.rt->memory().store8(drive, 'x');
    wrong.rt->cpu().gpr[4] = 1; wrong.rt->cpu().gpr[5] = drive;
    wrong.rt->invoke_import("sceUmdUser", kUmdActivateNid, wrong.rt->cpu());
    require(wrong.rt->cpu().gpr[2] == 0x80010016u && !wrong.io->umd_activated,
            "Unsupported UMD drive accepted");
}

void umd_timed_wait_contracts() {
    for (const auto duration : {1u, 16683u, 16684u, 50050u, 96000u}) {
        auto rt = std::make_unique<Runtime>();
        auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
        auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, 0x08001000u, 0x08010000u,
            0x08001000u, 0x08002000u, 0x08003000u, 32, 4096}, allocator);
        auto interrupts = register_interrupt_hle(*rt, 0x08001000u, 0x08002000u, 0x08003000u);
        auto delivery = register_interrupt_delivery(*rt, interrupts, threads, allocator);
        auto display = register_display_hle(*rt, threads, delivery, 8);
        auto io = register_io_startup_hle(*rt, threads, display);
        const char name[] = "disc0:";
        for (unsigned i = 0; i < sizeof(name); ++i) rt->memory().store8(0x08004000u + i, name[i]);
        auto& c = rt->cpu(); c.gpr[4] = 1; c.gpr[5] = 0x08004000u;
        rt->invoke_import("sceUmdUser", kUmdActivateNid, c);
        c.pc = 0x08001000u; c.gpr[31] = 0x08001004u;
        c.gpr[4] = 0x20; c.gpr[5] = duration; c.gpr[16] = 42; c.vfpu[127] = 123;
        rt->register_function(0x08001004u, [](auto& runtime, auto&) {
            runtime.stop("synthetic UMD timeout complete");
        }, "synthetic_timeout_return");
        rt->invoke_import("sceUmdUser", kUmdWaitDriveStatCBNid, c);
        require(!rt->stopped() && threads->current_uid() == 0 && display->clock.ticks() == 0,
                "UMD timeout returned immediately");
        const auto deadline = static_cast<std::uint64_t>(duration) * 18;
        if (deadline < DisplayClock::ticks_per_frame) {
            bool rejected = false;
            try { display->advance(*rt, deadline + 1); }
            catch (const std::exception&) { rejected = true; }
            require(rejected && display->clock.ticks() == 0 && threads->current_uid() == 0,
                    "Clock skipped a timeout or mutated on rejection");
        }
        display->advance(*rt, 1);
        require(threads->current_uid() == 0, "UMD timeout woke early");
        rt->run(c.pc, 32);
        require(rt->stop_reason() == "synthetic UMD timeout complete" &&
                display->clock.ticks() == deadline && c.gpr[2] == 0x800201A8u &&
                c.gpr[16] == 42 && c.vfpu[127] == 123 &&
                threads->current_uid() == kBootstrapUid && !threads->next_timeout(),
                "UMD timeout deadline, result or context restoration");
    }
    for (unsigned scenario = 0; scenario < 6; ++scenario) {
        Fixture f; auto& c = f.rt->cpu();
        auto interrupts = register_interrupt_hle(*f.rt, 0x08804000u, 0x08C00000u, 0x08805000u);
        auto delivery = register_interrupt_delivery(*f.rt, interrupts, f.threads,
            std::make_shared<GuestAllocator>(0x09020000u, 0x09030000u));
        auto display = register_display_hle(*f.rt, f.threads, delivery, 1);
        auto io = register_io_startup_hle(*f.rt, f.threads, display);
        io->umd_activated = scenario != 1;
        c.gpr[4] = scenario == 2 ? 0x10 : 0x20;
        c.gpr[5] = scenario == 3 ? 0 : 96000;
        if (scenario == 4) f.threads->enter_interrupt();
        if (scenario == 5) f.rt->invoke_import("Kernel_Library", 0x092968F4u, c);
        f.rt->invoke_import("sceUmdUser", kUmdWaitDriveStatCBNid, c);
        require(f.rt->stopped() && f.threads->current_uid() == kBootstrapUid &&
                !f.threads->next_timeout() && display->clock.ticks() == 0,
                "Rejected UMD wait changed scheduler or clock");
        if (scenario == 0) require(f.rt->stop_reason() == "UMD wait callback delivery is unimplemented",
                                   "Owned callback was silently ignored");
    }
    // No implicit clock is supplied by the legacy registration path.
    Fixture f; auto& c = f.rt->cpu(); c.gpr[4] = 0x20; c.gpr[5] = 96000;
    f.rt->invoke_import("sceUmdUser", kUmdWaitDriveStatCBNid, c);
    require(f.rt->stopped() && f.threads->current_uid() == kBootstrapUid &&
            !f.threads->next_timeout(), "Unsupported UMD wait mutated scheduler");
}

int main() {
    try { umd_timed_wait_contracts(); registration_contract(); rejection_contracts(); wlan_address_contract(); umd_activation_contract();
          std::cout << "Source-only startup I/O contracts passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
