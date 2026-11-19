#pragma once

#include "psprecomp/runtime.hpp"
#include <cstdint>
#include <memory>

namespace azazel6 {
struct StartupAvModules {
    std::uint32_t loaded_mask = 0;
    std::uint32_t successful_loads = 0;
    std::uint32_t duplicate_loads = 0;
    [[nodiscard]] bool loaded(std::uint32_t id) const noexcept {
        return id >= 0x300u && id <= 0x302u &&
            (loaded_mask & (1u << (id - 0x300u))) != 0;
    }
};

[[nodiscard]] std::shared_ptr<StartupAvModules> register_startup_av_modules_hle(psprecomp::Runtime&);
}
