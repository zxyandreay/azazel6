#include "azazel6/bootstrap.hpp"
#include "azazel6/io.hpp"
#include "media_fixture.hpp"
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

void disc_read_contract(const std::shared_ptr<IsoMedia>& media) {
    for (unsigned scenario = 0; scenario < 4; ++scenario) {
        Fixture f;
        auto& rt = *f.rt;
        auto& c = rt.cpu();
        auto partition = register_memory_hle(rt, std::make_shared<GuestAllocator>(0x09020000u, 0x09030000u));
        auto io = register_io_startup_hle(rt, f.threads, {}, media, partition);
        io->umd_activated = true;
        constexpr char path[] = "disc0:/PSP_GAME/USRDIR/DATA.BIN";
        constexpr std::uint32_t name = 0x08811000u;
        for (unsigned i = 0; i < sizeof(path); ++i) rt.memory().store8(name + i, path[i]);
        c.gpr[4] = 2; c.gpr[5] = name; c.gpr[6] = 0; c.gpr[7] = 256; c.gpr[8] = 0;
        rt.invoke_import("SysMemUserForUser", 0x237DBD4Fu, c);
        const auto block = c.gpr[2];
        const auto base = partition->blocks.at(block).base;
        c.gpr[4] = name; c.gpr[5] = 1; c.gpr[6] = 0;
        rt.invoke_import("IoFileMgrForUser", kIoOpenNid, c);
        const auto fd = c.gpr[2];
        auto read = [&](std::uint32_t address, std::uint32_t count) {
            c.gpr[4] = fd; c.gpr[5] = address; c.gpr[6] = count;
            rt.invoke_import("IoFileMgrForUser", kIoReadNid, c);
            return c.gpr[2];
        };
        auto seek = [&](std::int64_t offset, std::uint32_t whence) {
            const auto bits = static_cast<std::uint64_t>(offset);
            c.gpr[4] = fd; c.gpr[6] = static_cast<std::uint32_t>(bits);
            c.gpr[7] = static_cast<std::uint32_t>(bits >> 32); c.gpr[8] = whence;
            rt.invoke_import("IoFileMgrForUser", kIoLseekNid, c);
            return std::uint64_t(c.gpr[2]) | (std::uint64_t(c.gpr[3]) << 32);
        };
        rt.memory().store32(base, 0xCCCCCCCCu);
        if (scenario == 0) {
            require(read(0, 0) == 0 && read(base, 1) == 1 && read(base + 1, 8) == 3 &&
                    rt.memory().load32(base) == 0x74736574u && !rt.memory().load8(base + 4) &&
                    read(base, 1) == 0 && io->files.at(fd).position == 4 &&
                    io->bytes_read == 4 && !rt.stopped(), "Sequential and EOF disc reads");
            require(seek(-1, 2) == 3 && read(base, 1) == 1 && rt.memory().load8(base) == 't' &&
                    seek(0, 0) == 0 && io->seeks == 2 && io->bytes_read == 5,
                    "Disc seek origins and resumed read");
            const auto stack = f.threads->current_thread()->stack_base;
            require(read(stack, 4) == 4 && rt.memory().load32(stack) == 0x74736574u &&
                    seek(0, 0) == 0 && io->seeks == 3 && io->bytes_read == 9,
                    "Disc read into live caller stack");
            require(seek(-1, 0) == 0xFFFFFFFF80010016ull && io->files.at(fd).position == 0 &&
                    io->seeks == 3, "Negative disc seek changed position");
            constexpr std::int64_t max_offset = INT64_MAX;
            require(seek(max_offset, 0) == static_cast<std::uint64_t>(max_offset) &&
                    seek(1, 1) == 0xFFFFFFFF80010016ull &&
                    io->files.at(fd).position == static_cast<std::uint64_t>(max_offset),
                    "Overflowing signed seek changed position");
            c.gpr[4] = fd;
            rt.invoke_import("IoFileMgrForUser", kIoCloseNid, c);
            require(read(base, 4) == 0x80010009u && seek(0, 0) == 0xFFFFFFFF80010009ull &&
                    io->bytes_read == 9, "Stale disc handle");
            continue;
        }
        if (scenario == 1) {
            c.gpr[4] = block;
            rt.invoke_import("SysMemUserForUser", 0xB6D61D02u, c);
            read(base, 4);
        } else if (scenario == 2) {
            read(base, 257);
        } else {
            read(0xFFFFFFFEu, 4);
        }
        require(rt.stopped() && rt.memory().load32(base) == 0xCCCCCCCCu &&
                io->files.at(fd).position == 0 && !io->bytes_read, "Rejected read mutated state");
    }
}

void mounted_disc_contracts() {
    media_fixture::File image;
    auto media = std::make_shared<IsoMedia>(image.path, image.sha);
    disc_read_contract(media);
    Fixture f; auto& c = f.rt->cpu();
    auto io = register_io_startup_hle(*f.rt, f.threads, {}, media);
    const char path[] = "disc0:/PSP_GAME/USRDIR/DATA.BIN";
    for (unsigned i = 0; i < sizeof(path); ++i) f.rt->memory().store8(0x08811000 + i, path[i]);
    c.gpr[4] = 0x08811000; c.gpr[5] = 1; c.gpr[6] = 0;
    f.rt->invoke_import("IoFileMgrForUser", kIoOpenNid, c);
    require(c.gpr[2] == 0x80010013u && io->files.empty(), "Inactive disc opened");
    io->umd_activated = true;
    c.gpr[6] = 0644; // The file request worker supplies creation permissions even on read-only opens.
    f.rt->invoke_import("IoFileMgrForUser", kIoOpenNid, c);
    const auto fd = c.gpr[2];
    require(fd == 0x74000000u && io->opens == 1 && io->files.size() == 1, "Mounted disc open");
    c.gpr[4] = fd; c.gpr[5] = 0x01020006; c.gpr[6] = 0; c.gpr[7] = 0; c.gpr[8] = c.gpr[29]; c.gpr[9] = 4;
    f.rt->invoke_import("IoFileMgrForUser", kIoIoctlNid, c);
    require(!f.rt->stopped() && c.gpr[2] == 0 && f.rt->memory().load32(c.gpr[29]) == 26 &&
            io->sector_queries == 1, "Disc first-sector ioctl");
    f.rt->invoke_import("IoFileMgrForUser", kIoCloseNid, c);
    require(c.gpr[2] == 0 && io->files.empty(), "Disc close");
    f.rt->invoke_import("IoFileMgrForUser", kIoCloseNid, c);
    require(c.gpr[2] == 0x80010009u, "Stale file handle accepted");
    f.rt->invoke_import("IoFileMgrForUser", kIoIoctlNid, c);
    require(c.gpr[2] == 0x80010009u && io->sector_queries == 1, "Stale ioctl wrote output");
    const auto output = f.threads->current_thread()->stack_base;
    auto& memory = f.rt->memory();
    for (unsigned i = 0; i < 88; i += 4) memory.store32(output + i, 0xCCCCCCCCu);
    c.gpr[4] = 0x08811000; c.gpr[5] = output;
    f.rt->invoke_import("IoFileMgrForUser", kIoGetstatNid, c);
    require(!f.rt->stopped() && c.gpr[2] == 0 && io->stat_queries == 1 &&
            memory.load32(output) == (0x2000 | 0555) && memory.load32(output + 4) == 0x20 &&
            memory.load32(output + 8) == 4 && !memory.load32(output + 12) &&
            memory.load32(output + 64) == 26, "Incorrect disc stat");
    const std::array<std::uint16_t, 6> expected_date{2000, 2, 29, 23, 15, 22};
    for (unsigned offset : {16u, 32u, 48u}) {
        for (unsigned i = 0; i < expected_date.size(); ++i)
            require(memory.load16(output + offset + 2 * i) == expected_date[i], "Incorrect UTC recording date");
        require(!memory.load32(output + offset + 12), "Incorrect stat microseconds");
    }
    for (unsigned i = 68; i < 88; i += 4)
        require(memory.load32(output + i) == 0xCCCCCCCCu, "Stat clobbered reserved fields");
    memory.store8(0x08811000 + sizeof(path) - 2, 'X');
    memory.store32(output, 0xCCCCCCCCu);
    f.rt->invoke_import("IoFileMgrForUser", kIoGetstatNid, c);
    require(c.gpr[2] == 0x80010002u && memory.load32(output) == 0xCCCCCCCCu &&
            io->stat_queries == 1, "Missing stat mutated output");
    constexpr char savedata[] = "ms0:/PSP/SAVEDATA/ULUS10466DATA";
    for (unsigned i = 0; i < sizeof(savedata); ++i) memory.store8(0x08811000 + i, savedata[i]);
    c.gpr[4] = 0x08811000; c.gpr[5] = output;
    f.rt->invoke_import("IoFileMgrForUser", kIoGetstatNid, c);
    require(c.gpr[2] == 0x80010002u && memory.load32(output) == 0xCCCCCCCCu &&
            io->stat_queries == 1, "Absent Memory Stick stat mutated output");
    for (unsigned i = 0; i < sizeof(path); ++i) memory.store8(0x08811000 + i, path[i]);
    c.gpr[5] = output + f.threads->current_thread()->stack_size - 84;
    memory.store32(c.gpr[5], 0xCCCCCCCCu);
    f.rt->invoke_import("IoFileMgrForUser", kIoGetstatNid, c);
    require(f.rt->stopped() && memory.load32(c.gpr[5]) == 0xCCCCCCCCu &&
            io->stat_queries == 1, "Cross-stack stat wrote partial output");
    for (unsigned scenario = 0; scenario < 8; ++scenario) {
        Fixture bad;
        auto rejected = register_io_startup_hle(*bad.rt, bad.threads, {}, media);
        rejected->umd_activated = true;
        auto& rc = bad.rt->cpu();
        for (unsigned i = 0; i < sizeof(path); ++i) bad.rt->memory().store8(0x08811000 + i, path[i]);
        rc.gpr[4] = 0x08811000; rc.gpr[5] = 1; rc.gpr[6] = 0;
        if (scenario == 0) rc.gpr[5] = 3;
        if (scenario == 1) rc.gpr[4] = 0xFFFFFFFFu;
        if (scenario == 2) bad.rt->memory().store8(0x08811000, 'x');
        if (scenario == 7) rc.gpr[6] = 0777;
        bad.rt->invoke_import("IoFileMgrForUser", kIoOpenNid, rc);
        if (scenario < 3 || scenario == 7) {
            require(bad.rt->stopped() && rejected->files.empty(), "Invalid disc open mutated handles");
            continue;
        }
        rc.gpr[4] = rc.gpr[2]; rc.gpr[5] = 0x01020006; rc.gpr[6] = 0; rc.gpr[7] = 0;
        rc.gpr[8] = rc.gpr[29]; rc.gpr[9] = 4;
        const auto output = rc.gpr[29]; bad.rt->memory().store32(output, 0xDEADBEEF);
        if (scenario == 3) rc.gpr[8] = 0xFFFFFFFCu;
        if (scenario == 4) rc.gpr[9] = 8;
        if (scenario == 5) rc.gpr[5] = 0;
        if (scenario == 6) rc.gpr[8] = bad.threads->current_thread()->stack_base - 4;
        bad.rt->invoke_import("IoFileMgrForUser", kIoIoctlNid, rc);
        require(bad.rt->stopped() && !rejected->sector_queries && bad.rt->memory().load32(output) == 0xDEADBEEF,
                "Invalid ioctl mutated output");
    }
    auto rt = std::make_unique<Runtime>();
    auto threads = register_thread_hle(*rt, {0x09000000u, 0x09010000u, 0x08804000u,
        0x08C00000u, 0x08804000u, 0x08C00000u, 0x08805000u, 40u, 0x4000u});
    auto interrupts = register_interrupt_hle(*rt, 0x08804000u, 0x08C00000u, 0x08805000u);
    auto delivery = register_interrupt_delivery(*rt, interrupts, threads,
        std::make_shared<GuestAllocator>(0x09020000u, 0x09030000u));
    auto display = register_display_hle(*rt, threads, delivery, 1);
    auto mounted = register_io_startup_hle(*rt, threads, display, media);
    mounted->umd_activated = true;
    auto& rc = rt->cpu(); rc.gpr[4] = 0x20; rc.gpr[5] = 96000;
    rt->invoke_import("sceUmdUser", kUmdWaitDriveStatCBNid, rc);
    require(!rt->stopped() && rc.gpr[2] == 0 && !threads->next_timeout() &&
            threads->current_uid() == kBootstrapUid && display->clock.ticks() == 0,
            "Mounted readable disc must not schedule an empty-drive timeout");
}

int main() {
    try { mounted_disc_contracts(); umd_timed_wait_contracts(); registration_contract(); rejection_contracts(); wlan_address_contract(); umd_activation_contract();
          std::cout << "Synthetic startup I/O and mounted disc contracts passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
