#pragma once
#include "azazel6/threads.hpp"
#include "azazel6/display.hpp"
#include "azazel6/media.hpp"
#include "psprecomp/runtime.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace azazel6 {
inline constexpr std::uint32_t kIoDevctlNid = 0x54F5FB11u;
inline constexpr std::uint32_t kFatmsRegisterCallbackCommand = 0x02415821u;
inline constexpr std::uint32_t kWlanGetEtherAddrNid = 0x0C622081u;
inline constexpr std::uint32_t kUmdWaitDriveStatCBNid = 0x4A9E5E29u;
inline constexpr std::uint32_t kUmdActivateNid = 0xC6183D47u;
inline constexpr std::uint32_t kIoOpenNid = 0x109F50BCu, kIoCloseNid = 0x810C4BC3u;
inline constexpr std::uint32_t kIoIoctlNid = 0x63632449u;
inline constexpr std::uint32_t kIoGetstatNid = 0xACE946E8u;

struct DiscHandle { DiscFile file; std::uint64_t position = 0; };
// Runtime-local, immutable mounted media; no host-directory mapping or writes.
struct IoStartupState {
    const psprecomp::Runtime* owner = nullptr;
    std::shared_ptr<IsoMedia> media;
    std::map<std::uint32_t, DiscHandle> files;
    std::uint32_t next_fd = 0x74000000u;
    std::uint64_t opens = 0, sector_queries = 0, stat_queries = 0;
    std::vector<std::uint32_t> fatms_callbacks;
    std::array<std::uint8_t, 6> wlan_address{0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    std::uint32_t wlan_address_reads = 0;
    bool umd_activated = false;
    std::uint32_t umd_activation_count = 0;
};

[[nodiscard]] std::shared_ptr<IoStartupState> register_io_startup_hle(
    psprecomp::Runtime&, std::shared_ptr<ThreadRegistry>, std::shared_ptr<DisplayState> = {},
    std::shared_ptr<IsoMedia> = {});
} // namespace azazel6
