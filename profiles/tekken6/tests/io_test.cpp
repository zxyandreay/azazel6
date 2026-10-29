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

int main() {
    try { registration_contract(); rejection_contracts(); wlan_address_contract();
          std::cout << "Source-only startup I/O contracts passed\n"; }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
