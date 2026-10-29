#pragma once
#include "azazel6/threads.hpp"
#include "azazel6/display.hpp"
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

// Device bookkeeping for the source-only startup experiment. No host path,
// file, capacity, insertion state or callback delivery is modeled here.
struct IoStartupState {
    std::vector<std::uint32_t> fatms_callbacks;
    std::array<std::uint8_t, 6> wlan_address{0x02, 0x00, 0x00, 0x00, 0x00, 0x01};
    std::uint32_t wlan_address_reads = 0;
    bool umd_activated = false;
    std::uint32_t umd_activation_count = 0;
};

[[nodiscard]] std::shared_ptr<IoStartupState> register_io_startup_hle(
    psprecomp::Runtime&, std::shared_ptr<ThreadRegistry>, std::shared_ptr<DisplayState> = {});
} // namespace azazel6
