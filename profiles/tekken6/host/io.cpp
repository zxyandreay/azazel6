#include "azazel6/io.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>

namespace azazel6 {
std::shared_ptr<IoStartupState> register_io_startup_hle(
        psprecomp::Runtime& rt, std::shared_ptr<ThreadRegistry> threads, std::shared_ptr<DisplayState> display) {
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
    rt.nids().add("sceWlanDrv", kWlanGetEtherAddrNid, "sceWlanGetEtherAddr");
    rt.register_hle("sceWlanDrv", kWlanGetEtherAddrNid,
        [state, threads](auto& runtime, auto& c) {
            const auto current = threads->current_thread();
            const auto end = static_cast<std::uint64_t>(c.gpr[4]) + state->wlan_address.size();
            if (!current || current->status != ThreadStatus::Running ||
                    !threads->interrupts_enabled() || c.gpr[4] < current->stack_base ||
                    end > static_cast<std::uint64_t>(current->stack_base) + current->stack_size) {
                runtime.stop("WLAN address output requires the live caller stack"); return;
            }
            for (std::size_t i = 0; i < state->wlan_address.size(); ++i)
                runtime.memory().store8(c.gpr[4] + static_cast<std::uint32_t>(i), state->wlan_address[i]);
            ++state->wlan_address_reads;
            c.set_gpr(2, 0);
        });
    rt.nids().add("sceUmdUser", kUmdActivateNid, "sceUmdActivate");
    rt.register_hle("sceUmdUser", kUmdActivateNid,
        [state, threads](auto& runtime, auto& c) {
            const auto current = threads->current_thread();
            if (!current || current->status != ThreadStatus::Running || !threads->interrupts_enabled()) {
                runtime.stop("UMD activation requires a running thread with interrupts enabled"); return;
            }
            if (c.gpr[4] < 1 || c.gpr[4] > 2) { c.set_gpr(2, 0x80010016u); return; }
            if (c.gpr[4] != 1) { runtime.stop("Unsupported UMD activation mode"); return; }
            constexpr std::array<std::uint8_t, 7> drive{'d', 'i', 's', 'c', '0', ':', 0};
            const auto end = static_cast<std::uint64_t>(c.gpr[5]) + drive.size();
            const auto ram_end = psprecomp::GuestMemory::kPhysicalBase +
                static_cast<std::uint64_t>(runtime.memory().size());
            if (c.gpr[5] < psprecomp::GuestMemory::kPhysicalBase || end > ram_end) {
                c.set_gpr(2, 0x80010016u); return;
            }
            for (std::size_t i = 0; i < drive.size(); ++i)
                if (runtime.memory().load8(c.gpr[5] + static_cast<std::uint32_t>(i)) != drive[i]) {
                    c.set_gpr(2, 0x80010016u); return;
                }
            state->umd_activated = true;
            ++state->umd_activation_count;
            c.set_gpr(2, 0);
            // Activation records the empty drive mount only. No medium, host path,
            // filesystem or readable state is supplied by this source-only host.
        });
    rt.nids().add("sceUmdUser", kUmdWaitDriveStatCBNid, "sceUmdWaitDriveStatCB");
    rt.register_hle("sceUmdUser", kUmdWaitDriveStatCBNid,
        [state, threads, display](auto& runtime, auto& c) {
            const auto current = threads->current_thread();
            if (!current || current->status != ThreadStatus::Running || threads->in_interrupt() ||
                !threads->interrupts_enabled()) {
                runtime.stop("UMD wait requires an interruptible running thread"); return;
            }
            // Only the observed finite READY wait on an activated empty drive.
            if (!display || !state->umd_activated || c.gpr[4] != 0x20 || !c.gpr[5]) {
                runtime.stop("Unsupported UMD drive-state wait configuration"); return;
            }
            for (const auto& [uid, callback] : threads->callbacks()) {
                if (callback.owner == current->uid) {
                    runtime.stop("UMD wait callback delivery is unimplemented"); return;
                }
            }
            display->wait_timeout(runtime, c, c.gpr[5], 0x800201A8u);
        });
    return state;
}
} // namespace azazel6
