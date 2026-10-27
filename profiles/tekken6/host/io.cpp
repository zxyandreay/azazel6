#include "azazel6/io.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>

namespace azazel6 {
std::shared_ptr<IoStartupState> register_io_startup_hle(
        psprecomp::Runtime& rt, std::shared_ptr<ThreadRegistry> threads) {
    if (!threads) throw psprecomp::Error("I/O startup HLE requires thread ownership");
    auto state = std::make_shared<IoStartupState>();
    rt.nids().add("IoFileMgrForUser", kIoDevctlNid, "sceIoDevctl");
    rt.register_hle("IoFileMgrForUser", kIoDevctlNid,
        [state, threads](auto& runtime, auto& c) {
            constexpr char device[] = "fatms0:";
            if (!runtime.memory().contains(c.gpr[4], sizeof(device))) {
                runtime.stop("sceIoDevctl device name outside canonical RAM"); return;
            }
            for (std::size_t i = 0; i < sizeof(device); ++i) {
                if (runtime.memory().load8(c.gpr[4] + static_cast<std::uint32_t>(i)) !=
                        static_cast<std::uint8_t>(device[i])) {
                    runtime.stop("Unsupported sceIoDevctl device"); return;
                }
            }
            if (c.gpr[5] != kFatmsRegisterCallbackCommand) {
                runtime.stop("Unsupported fatms0 sceIoDevctl command"); return;
            }
            if (c.gpr[7] != 4 || c.gpr[8] != 0 || c.gpr[9] != 0 || (c.gpr[6] & 3u)) {
                runtime.stop("Invalid fatms0 callback registration ABI"); return;
            }
            const auto current = threads->current_thread();
            const auto input_end = static_cast<std::uint64_t>(c.gpr[6]) + 4;
            if (!current || current->status != ThreadStatus::Running || !threads->interrupts_enabled() ||
                    c.gpr[6] < current->stack_base ||
                    input_end > static_cast<std::uint64_t>(current->stack_base) + current->stack_size) {
                runtime.stop("fatms0 callback input requires the live caller stack"); return;
            }
            const auto uid = runtime.memory().load32(c.gpr[6]);
            const auto callback = threads->callbacks().find(uid);
            if (callback == threads->callbacks().end() || callback->second.owner != current->uid ||
                    std::find(state->fatms_callbacks.begin(), state->fatms_callbacks.end(), uid) !=
                        state->fatms_callbacks.end()) {
                c.set_gpr(2, 0x80010016u); return;
            }
            state->fatms_callbacks.push_back(uid);
            c.set_gpr(2, 0);
        });
    return state;
}
} // namespace azazel6
