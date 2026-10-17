#pragma once
#include "psprecomp/runtime.hpp"

namespace azazel6 {
inline constexpr std::uint32_t kMainModuleUid = 0x20000000u;
// A single, already validated resident image. This does not load guest modules.
void register_module_hle(psprecomp::Runtime&, std::uint32_t begin, std::uint32_t size);
}
