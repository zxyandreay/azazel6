#pragma once
#include "psprecomp/runtime.hpp"
#include <memory>

namespace azazel6 {
struct LanguageMode {
    std::uint32_t language, confirm_button;
    std::uint64_t reads = 0, writes = 0;
};
// Process-local guest settings, initialized explicitly by the game profile.
std::shared_ptr<LanguageMode> register_language_hle(psprecomp::Runtime&, LanguageMode,
                                                  std::uint32_t data_begin, std::uint32_t data_end);
}
