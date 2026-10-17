#pragma once
#include "psprecomp/runtime.hpp"
#include <cstdint>
#include <memory>
#include <string_view>

namespace azazel6 {
inline constexpr std::uint32_t kSdk500550Nid = 0x91DE343Cu;
inline constexpr std::uint32_t kCompilerVersionNid = 0xF77D77CBu;

// Only the startup metadata implemented here; not a complete SceGameInfo model.
struct BootstrapState {
    std::uint32_t game_info_flags{};
    std::uint32_t compiler_version{};
    std::uint32_t last_sdk_argument{}; // Diagnostic input, not the PSP SDK getter's value.
    std::uint32_t completed_calls{};
};

[[nodiscard]] std::shared_ptr<BootstrapState> register_bootstrap_hle(psprecomp::Runtime& runtime);
void invoke_returning_import(psprecomp::Runtime& runtime, psprecomp::AllegrexContext& context,
                             std::string_view library, std::uint32_t nid);
} // namespace azazel6
