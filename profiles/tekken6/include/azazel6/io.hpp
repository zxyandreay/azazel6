#pragma once
#include "azazel6/threads.hpp"
#include "psprecomp/runtime.hpp"
#include <cstdint>
#include <memory>
#include <vector>

namespace azazel6 {
inline constexpr std::uint32_t kIoDevctlNid = 0x54F5FB11u;
inline constexpr std::uint32_t kFatmsRegisterCallbackCommand = 0x02415821u;

// Only callback ownership for the source-only startup experiment. No host path,
// file, capacity, insertion state or callback delivery is modeled here.
struct IoStartupState { std::vector<std::uint32_t> fatms_callbacks; };

[[nodiscard]] std::shared_ptr<IoStartupState> register_io_startup_hle(
    psprecomp::Runtime&, std::shared_ptr<ThreadRegistry>);
} // namespace azazel6
