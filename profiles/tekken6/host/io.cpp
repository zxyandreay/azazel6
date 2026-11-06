#include "azazel6/io.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>
#include <limits>

namespace azazel6 {
namespace {
constexpr std::uint32_t bad_fd = 0x80010009u, missing = 0x80010002u;
bool io_caller(psprecomp::Runtime& runtime, psprecomp::AllegrexContext& c,
               const IoStartupState& state, const ThreadRegistry& threads) {
    const auto current = threads.current_thread();
    if (state.owner != &runtime || &c != &runtime.cpu() || !current ||
        current->status != ThreadStatus::Running || threads.in_interrupt() || !threads.interrupts_enabled()) {
        runtime.stop("Disc I/O requires its live interruptible caller"); return false;
    }
    return true;
}
std::string disc_path(psprecomp::Runtime& runtime, std::uint32_t address) {
    const auto end = psprecomp::GuestMemory::kPhysicalBase + std::uint64_t(runtime.memory().size());
    std::string result;
    for (unsigned i = 0; i < 1024; ++i) {
        const auto at = std::uint64_t(address) + i;
        if (at < psprecomp::GuestMemory::kPhysicalBase || at >= end)
            throw psprecomp::Error("Disc path outside canonical RAM");
        const auto ch = runtime.memory().load8(static_cast<std::uint32_t>(at));
        if (!ch) return result;
        result += static_cast<char>(ch);
    }
    throw psprecomp::Error("Unterminated disc path");
}
bool absent_memory_stick_path(const std::string& path) {
    if (!path.starts_with("ms0:/")) return false;
    if (path.size() <= 5 || path.size() > 1024) throw psprecomp::Error("Invalid Memory Stick path length");
    for (std::size_t begin = 5; begin < path.size();) {
        const auto end = path.find('/', begin);
        const auto component = path.substr(begin, end == std::string::npos ? end : end - begin);
        if (component.empty() || component == "." || component == ".." ||
                !std::all_of(component.begin(), component.end(), [](unsigned char ch) {
                    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z') ||
                        (ch >= '0' && ch <= '9') || ch == '_' || ch == '.' || ch == '-';
                })) throw psprecomp::Error("Invalid Memory Stick path component");
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return true;
}
}
std::shared_ptr<IoStartupState> register_io_startup_hle(
        psprecomp::Runtime& rt, std::shared_ptr<ThreadRegistry> threads, std::shared_ptr<DisplayState> display,
        std::shared_ptr<IsoMedia> media, std::shared_ptr<const PartitionMemory> partition) {
    if (!threads) throw psprecomp::Error("I/O startup HLE requires thread ownership");
    if (partition && partition->owner != &rt) throw psprecomp::Error("Disc I/O partition belongs to another runtime");
    auto state = std::make_shared<IoStartupState>();
    state->owner = &rt; state->media = std::move(media);
    rt.nids().add("IoFileMgrForUser", kIoOpenNid, "sceIoOpen");
    rt.register_hle("IoFileMgrForUser", kIoOpenNid, [state, threads](auto& runtime, auto& c) {
        if (!io_caller(runtime, c, *state, *threads)) return;
        // Startup uses mode 0 or 0644. Creation permissions have no effect on a read-only open.
        if (c.gpr[5] != 1 || (c.gpr[6] != 0 && c.gpr[6] != 0644)) {
            runtime.stop("Unsupported disc open flags or mode"); return;
        }
        if (!state->media || !state->umd_activated) { c.set_gpr(2, 0x80010013u); return; }
        try {
            const auto path = disc_path(runtime, c.gpr[4]);
            const auto file = state->media->find(path);
            if (!file || file->directory) {
                state->last_missing_path = path; ++state->missing_opens;
                c.set_gpr(2, missing); return;
            }
            if (state->files.size() >= 64 || state->next_fd >= 0x75000000u) { c.set_gpr(2, 0x80010018u); return; }
            const auto fd = state->next_fd++;
            state->files.emplace(fd, DiscHandle{*file}); ++state->opens;
            c.set_gpr(2, fd);
        } catch (const std::exception& error) { runtime.stop(error.what()); }
    });
    rt.nids().add("IoFileMgrForUser", kIoReadNid, "sceIoRead");
    rt.register_hle("IoFileMgrForUser", kIoReadNid, [state, threads, partition](auto& runtime, auto& c) {
        if (!io_caller(runtime, c, *state, *threads)) return;
        auto handle = state->files.find(c.gpr[4]);
        if (handle == state->files.end()) { c.set_gpr(2, bad_fd); return; }
        const auto address = c.gpr[5], count = c.gpr[6];
        if (!count) { c.set_gpr(2, 0); return; }
        const auto end = std::uint64_t(address) + count;
        const auto current = threads->current_thread();
        const bool stack_owned = address >= current->stack_base &&
            end <= std::uint64_t(current->stack_base) + current->stack_size;
        const bool partition_owned = partition && std::any_of(partition->blocks.begin(), partition->blocks.end(),
            [address, end](const auto& item) {
                const auto& block = item.second;
                return address >= block.base && end <= std::uint64_t(block.base) + block.size;
            });
        if ((!stack_owned && !partition_owned) || count > 8u * 1024 * 1024 ||
                !runtime.memory().contains(address, count)) {
            runtime.stop("Disc read requires one bounded live stack or partition buffer"); return;
        }
        try {
            auto& file = handle->second;
            const auto bytes = state->media->read(file.file, file.position, count);
            if (!bytes.empty()) runtime.memory().copy_in(address, bytes);
            file.position += bytes.size();
            ++state->reads;
            state->bytes_read += bytes.size();
            c.set_gpr(2, static_cast<std::uint32_t>(bytes.size()));
        } catch (const std::exception& error) { runtime.stop(error.what()); }
    });
    rt.nids().add("IoFileMgrForUser", kIoGetstatNid, "sceIoGetstat");
    rt.register_hle("IoFileMgrForUser", kIoGetstatNid, [state, threads](auto& runtime, auto& c) {
        if (!io_caller(runtime, c, *state, *threads)) return;
        if (!state->media || !state->umd_activated) { c.set_gpr(2, 0x80010013u); return; }
        const auto out = c.gpr[5];
        const auto current = threads->current_thread();
        if ((out & 3) || out < current->stack_base ||
            std::uint64_t(out) + 88 > std::uint64_t(current->stack_base) + current->stack_size) {
            runtime.stop("Disc stat output requires live caller stack"); return;
        }
        try {
            const auto path = disc_path(runtime, c.gpr[4]);
            state->last_stat_path = path;
            if (absent_memory_stick_path(path)) { c.set_gpr(2, missing); return; }
            const auto file = state->media->find(path);
            if (!file) { c.set_gpr(2, missing); return; }
            auto& memory = runtime.memory();
            memory.store32(out, (file->directory ? 0x1000u : 0x2000u) | 0555u);
            memory.store32(out + 4, file->directory ? 0x10u : 0x20u);
            memory.store32(out + 8, file->size);
            memory.store32(out + 12, 0);
            for (unsigned at : {16u, 32u, 48u}) {
                for (unsigned i = 0; i < file->recorded_utc.size(); ++i)
                    memory.store16(out + at + i * 2, file->recorded_utc[i]);
                memory.store32(out + at + 12, 0);
            }
            memory.store32(out + 64, file->sector); // UMD private[0]; preserve private[1..5].
            ++state->stat_queries;
            c.set_gpr(2, 0);
        } catch (const std::exception& error) { runtime.stop(error.what()); }
    });
    rt.nids().add("IoFileMgrForUser", kIoLseekNid, "sceIoLseek");
    rt.register_hle("IoFileMgrForUser", kIoLseekNid, [state, threads](auto& runtime, auto& c) {
        if (!io_caller(runtime, c, *state, *threads)) return;
        const auto found = state->files.find(c.gpr[4]);
        if (found == state->files.end()) {
            c.set_gpr(2, bad_fd); c.set_gpr(3, 0xFFFFFFFFu); return;
        }
        auto& handle = found->second;
        std::uint64_t base{};
        if (c.gpr[8] == 1) base = handle.position;
        else if (c.gpr[8] == 2) base = handle.file.size;
        else if (c.gpr[8] != 0) {
            c.set_gpr(2, 0x80010016u); c.set_gpr(3, 0xFFFFFFFFu); return;
        }
        const auto bits = std::uint64_t(c.gpr[6]) | (std::uint64_t(c.gpr[7]) << 32);
        const auto offset = static_cast<std::int64_t>(bits);
        std::uint64_t target{};
        if (offset < 0) {
            const auto magnitude = std::uint64_t(-(offset + 1)) + 1;
            if (magnitude > base) {
                c.set_gpr(2, 0x80010016u); c.set_gpr(3, 0xFFFFFFFFu); return;
            }
            target = base - magnitude;
        } else {
            const auto positive = static_cast<std::uint64_t>(offset);
            if (positive > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) - base) {
                c.set_gpr(2, 0x80010016u); c.set_gpr(3, 0xFFFFFFFFu); return;
            }
            target = base + positive;
        }
        handle.position = target; ++state->seeks;
        c.set_gpr(2, static_cast<std::uint32_t>(target));
        c.set_gpr(3, static_cast<std::uint32_t>(target >> 32));
    });
    rt.nids().add("IoFileMgrForUser", kIoCloseNid, "sceIoClose");
    rt.register_hle("IoFileMgrForUser", kIoCloseNid, [state, threads](auto& runtime, auto& c) {
        if (!io_caller(runtime, c, *state, *threads)) return;
        c.set_gpr(2, state->files.erase(c.gpr[4]) ? 0 : bad_fd);
    });
    rt.nids().add("IoFileMgrForUser", kIoIoctlNid, "sceIoIoctl");
    rt.register_hle("IoFileMgrForUser", kIoIoctlNid, [state, threads](auto& runtime, auto& c) {
        if (!io_caller(runtime, c, *state, *threads)) return;
        const auto file = state->files.find(c.gpr[4]);
        if (file == state->files.end()) { c.set_gpr(2, bad_fd); return; }
        // Pinned PortableKit ioctl_op: 0x01020006 returns the file's first LBA.
        if (c.gpr[5] != 0x01020006u || c.gpr[6] || c.gpr[7] || c.gpr[9] != 4) {
            runtime.stop("Unsupported disc ioctl command or ABI"); return;
        }
        const auto current = threads->current_thread();
        const auto end = std::uint64_t(c.gpr[8]) + 4;
        if ((c.gpr[8] & 3) || c.gpr[8] < current->stack_base ||
            end > std::uint64_t(current->stack_base) + current->stack_size) {
            runtime.stop("Disc sector output requires live caller stack"); return;
        }
        runtime.memory().store32(c.gpr[8], file->second.file.sector);
        ++state->sector_queries; c.set_gpr(2, 0);
    });
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
            // A supplied medium was fully validated before guest execution.
        });
    rt.nids().add("sceUmdUser", kUmdWaitDriveStatCBNid, "sceUmdWaitDriveStatCB");
    rt.register_hle("sceUmdUser", kUmdWaitDriveStatCBNid,
        [state, threads, display](auto& runtime, auto& c) {
            const auto current = threads->current_thread();
            if (!current || current->status != ThreadStatus::Running || threads->in_interrupt() ||
                !threads->interrupts_enabled()) {
                runtime.stop("UMD wait requires an interruptible running thread"); return;
            }
            // Only the observed finite READY wait on an activated drive.
            if (!display || !state->umd_activated || c.gpr[4] != 0x20 || !c.gpr[5]) {
                runtime.stop("Unsupported UMD drive-state wait configuration"); return;
            }
            for (const auto& [uid, callback] : threads->callbacks()) {
                if (callback.owner == current->uid) {
                    runtime.stop("UMD wait callback delivery is unimplemented"); return;
                }
            }
            if (state->media) { c.set_gpr(2, 0); return; }
            display->wait_timeout(runtime, c, c.gpr[5], 0x800201A8u);
        });
    return state;
}
} // namespace azazel6
