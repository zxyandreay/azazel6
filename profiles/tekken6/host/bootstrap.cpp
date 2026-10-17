#include "azazel6/bootstrap.hpp"
#include "psprecomp/common.hpp"

namespace azazel6 {
std::shared_ptr<BootstrapState> register_bootstrap_hle(psprecomp::Runtime& runtime) {
    auto state = std::make_shared<BootstrapState>();
    runtime.nids().add("SysMemUserForUser", kSdk500550Nid, "sceKernelSetCompiledSdkVersion500_550");
    runtime.nids().add("SysMemUserForUser", kCompilerVersionNid, "sceKernelSetCompilerVersion");
    runtime.register_hle("SysMemUserForUser", kSdk500550Nid,
        [state](psprecomp::Runtime& rt, psprecomp::AllegrexContext& ctx) {
            const auto family = ctx.gpr[4] >> 16;
            if (family != 0x0500u && family != 0x0505u) {
                // Firmware's invalid-version path resets/hangs. Stop the experiment instead.
                rt.stop("Rejected SDK family: " + psprecomp::hex32(ctx.gpr[4]));
                return;
            }
            state->game_info_flags |= 0x1000u;
            state->last_sdk_argument = ctx.gpr[4];
            ++state->completed_calls;
            ctx.set_gpr(2, 0);
        });
    runtime.register_hle("SysMemUserForUser", kCompilerVersionNid,
        [state](psprecomp::Runtime&, psprecomp::AllegrexContext& ctx) {
            state->compiler_version = ctx.gpr[4];
            ++state->completed_calls;
            ctx.set_gpr(2, 0);
        });
    return state;
}

void invoke_returning_import(psprecomp::Runtime& runtime, psprecomp::AllegrexContext& context,
                             std::string_view library, std::uint32_t nid) {
    const auto caller = psprecomp::capture_runtime_execution_context();
    const auto stub = context.pc;
    const auto return_pc = context.gpr[31];
    runtime.invoke_import(library, nid, context);
    // A scheduling service may replace the context or redirect PC. Preserve that decision.
    if (!runtime.stopped() && context.pc == stub && psprecomp::runtime_execution_context_matches(caller))
        context.pc = return_pc;
}
} // namespace azazel6
