#pragma once
#include "psprecomp/runtime.hpp"
#include <memory>

namespace azazel6 {
class ThreadRegistry;
struct PartitionMemory;
void register_system_param_hle(psprecomp::Runtime&, std::shared_ptr<ThreadRegistry>,
    std::uint32_t confirm_button, std::int32_t timezone_minutes = 0,
    std::shared_ptr<const PartitionMemory> partition = {});
struct LanguageMode {
    std::uint32_t language, confirm_button;
    std::uint64_t reads = 0, writes = 0;
};
// Process-local guest settings, initialized explicitly by the game profile.
std::shared_ptr<LanguageMode> register_language_hle(psprecomp::Runtime&, LanguageMode,
                                                  std::uint32_t data_begin, std::uint32_t data_end);
}
