#include "azazel6/vfpu_vi2uc.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <stdexcept>

int main() {
    try {
        // Synthetic inputs and expected PSP autotest byte packing.
        struct Case { std::array<std::uint32_t, 4> input; std::uint32_t expected; };
        for (const auto& sample : std::array<Case, 4>{{
                 {{{0x01234567u, 0x89ABCDEFu, 0x01234567u, 0x89ABCDEFu}}, 0x00020002u},
                 {{{0xFEDCBA89u, 0u, 0u, 0u}}, 0u},
                 {{{0xFCCF00FFu, 0x1100FF11u, 0xFFFFFFFFu, 0x7F7F7F7Fu}}, 0xFE002200u},
                 {{{0xFFFFFFFFu, 0x7F7F7F7Fu, 0xFFFFFFFFu, 0x7F7F7F7Fu}}, 0xFE00FE00u}}}) {
            psprecomp::AllegrexContext ctx{};
            ctx.eat_vfpu_prefixes();
            for (std::uint32_t lane = 0u; lane < 4u; ++lane)
                ctx.vfpu[psprecomp::AllegrexContext::vfpu_vector_lane_index(0u, 4u, lane)] =
                    std::bit_cast<float>(sample.input[lane]);
            azazel6::execute_startup_vi2uc_q(ctx);
            if (ctx.vfpu_scalar_bits(96u) != sample.expected ||
                ctx.vfpu_ctrl[0] != 0xE4u || ctx.vfpu_ctrl[1] != 0xE4u || ctx.vfpu_ctrl[2] != 0u)
                throw std::runtime_error("vi2uc byte packing or prefix consumption failed");
        }
        psprecomp::AllegrexContext ctx{};
        ctx.eat_vfpu_prefixes();
        for (std::uint32_t lane = 0u; lane < 4u; ++lane)
            ctx.vfpu[psprecomp::AllegrexContext::vfpu_vector_lane_index(0u, 4u, lane)] =
                std::bit_cast<float>((lane + 1u) << 24u);
        ctx.vfpu_ctrl[0] = 0x1Bu; // Reverse the source lanes.
        azazel6::execute_startup_vi2uc_q(ctx);
        if (ctx.vfpu_scalar_bits(96u) != 0x02040608u)
            throw std::runtime_error("vi2uc source prefix failed");
        ctx.set_vfpu_scalar_bits(96u, 0xDEADBEEFu);
        ctx.vfpu_ctrl[2] = 1u << 8u; // Mask the destination lane.
        azazel6::execute_startup_vi2uc_q(ctx);
        if (ctx.vfpu_scalar_bits(96u) != 0xDEADBEEFu || ctx.vfpu_ctrl[2] != 0u)
            throw std::runtime_error("vi2uc destination mask failed");
        std::cout << "Synthetic startup vi2uc contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
