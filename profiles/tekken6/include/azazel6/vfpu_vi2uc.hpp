#pragma once

#include "psprecomp/allegrex_context.hpp"
#include <bit>
#include <cstdint>

namespace azazel6 {

// Exact ULUS10466 startup encoding D03C80E0: vi2uc.q S600, C000.
inline void execute_startup_vi2uc_q(psprecomp::AllegrexContext& ctx) noexcept {
    float lanes[4]{};
    ctx.read_vfpu_vector_with_source_prefix(lanes, 0u, 4u, 0u);
    std::uint32_t packed = 0u;
    for (std::uint32_t lane = 0u; lane < 4u; ++lane) {
        const auto value = std::bit_cast<std::int32_t>(lanes[lane]);
        const std::uint32_t byte = value < 0 ? 0u : (static_cast<std::uint32_t>(value) >> 23u) & 0xFFu;
        packed |= byte << (lane * 8u);
    }
    const float result = std::bit_cast<float>(packed);
    ctx.write_vfpu_vector_with_destination_prefix(&result, 96u, 1u);
}

}
