#include "azazel6/bootstrap.hpp"
#include <iostream>
#include <stdexcept>

using psprecomp::Runtime;
using psprecomp::AllegrexContext;
using namespace azazel6;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void settings_contracts() {
    auto rt = std::make_unique<Runtime>();
    auto state = register_bootstrap_hle(*rt);
    auto& ctx = rt->cpu();
    rt->memory().store32(0x08804000u, 0x12345678u);
    for (auto version : {0x05000000u, 0x0500FFFFu, 0x05050010u, 0x0505FFFFu}) {
        ctx.gpr.fill(0xA5A5A5A5u);
        ctx.gpr[0] = 0;
        ctx.gpr[4] = version;
        ctx.gpr[31] = 0x08805000u;
        ctx.pc = 0x08804000u;
        const auto before = ctx.gpr;
        invoke_returning_import(*rt, ctx, "SysMemUserForUser", kSdk500550Nid);
        require(!rt->stopped() && ctx.gpr[2] == 0 && ctx.pc == 0x08805000u, "SDK return ABI");
        require(state->game_info_flags == 0x1000u && state->last_sdk_argument == version, "SDK state");
        for (std::size_t i = 0; i < before.size(); ++i)
            if (i != 2) require(before[i] == ctx.gpr[i], "Unexpected register clobber");
    }
    for (auto version : {0u, 0x00030306u, 0xFFFFFFFFu}) {
        ctx.gpr[4] = version;
        rt->invoke_import("SysMemUserForUser", kCompilerVersionNid, ctx);
        require(state->compiler_version == version && state->game_info_flags == 0x1000u, "Compiler state");
        require(ctx.gpr[2] == 0, "Compiler return");
    }
    require(state->completed_calls == 7, "Call accounting");
    require(rt->memory().load32(0x08804000u) == 0x12345678u, "Guest memory changed");
    auto independent = std::make_unique<Runtime>();
    auto other = register_bootstrap_hle(*independent);
    require(other->completed_calls == 0 && other->compiler_version == 0, "Cross-runtime state leak");
    state.reset(); // Runtime callbacks own state, not a dangling reference to this handle.
    rt->invoke_import("SysMemUserForUser", kCompilerVersionNid, ctx);
    require(!rt->stopped(), "State lifetime");
}
void rejects_invalid() {
    for (auto version : {0u, 0x04FFFFFFu, 0x05010000u, 0x05060000u, 0xFFFFFFFFu}) {
        auto rt = std::make_unique<Runtime>();
        auto state = register_bootstrap_hle(*rt);
        auto& ctx = rt->cpu();
        ctx.pc = 0x08804000u;
        ctx.gpr[31] = 0x08805000u;
        ctx.gpr[2] = 0xDEADBEEFu;
        ctx.gpr[4] = version;
        invoke_returning_import(*rt, ctx, "SysMemUserForUser", kSdk500550Nid);
        require(rt->stopped() && ctx.pc == 0x08804000u && ctx.gpr[2] == 0xDEADBEEFu, "Invalid SDK accepted/returned");
        require(state->completed_calls == 0 && state->game_info_flags == 0, "Rejected SDK mutated state");
    }
    auto rt = std::make_unique<Runtime>();
    auto state = register_bootstrap_hle(*rt);
    auto& ctx = rt->cpu();
    ctx.pc = 0x08804000u;
    invoke_returning_import(*rt, ctx, "UnknownLibrary", kSdk500550Nid);
    require(rt->stopped() && state->completed_calls == 0 && ctx.pc == 0x08804000u, "Unknown library succeeded");
}
void returning_import_contracts() {
    auto rt = std::make_unique<Runtime>();
    auto& ctx = rt->cpu();
    ctx.pc = 0x08804000u;
    ctx.gpr[31] = 0x08805000u;
    rt->register_hle("synthetic", 1, [](Runtime&, AllegrexContext& c) { c.pc = 0x08806000u; });
    invoke_returning_import(*rt, ctx, "synthetic", 1);
    require(ctx.pc == 0x08806000u, "Redirect overwritten");
    ctx.pc = 0x08804000u;
    rt->register_hle("synthetic", 2, [](Runtime&, AllegrexContext&) {
        psprecomp::set_runtime_thread_identity(7, "synthetic");
    });
    invoke_returning_import(*rt, ctx, "synthetic", 2);
    require(ctx.pc == 0x08804000u, "Thread switch overwritten");
    psprecomp::set_runtime_thread_identity(-1, "");
}
int main() {
    try {
        settings_contracts(); rejects_invalid(); returning_import_contracts();
        std::cout << "Bootstrap HLE and return ABI contracts passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
