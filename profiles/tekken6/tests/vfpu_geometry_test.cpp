#include "psprecomp/runtime.hpp"
#include "psprecomp/interpreter.hpp"
#include <array>
#include <bit>
#include <iostream>
#include <memory>
#include <stdexcept>

int main() {
    try {
        // Synthetic affine depth projection. Matrix lanes not selected by the
        // pair dot product contain poison values; data is not from the game.
        struct Case { float depth, scale, offset; std::int32_t expected; };
        for (const auto& sample : std::array<Case, 8>{{
                 {4, 2, 3, 11}, {-4, 2, 3, -5}, {0, 1, 0.5f, 0},
                 {0, 1, 1.5f, 2}, {0, 1, 2.5f, 2}, {0, 1, -0.5f, 0},
                 {0, 1, -1.5f, -2}, {0.5f, 100, 7, 57}}}) {
            auto rt = std::make_unique<psprecomp::Runtime>();
            auto& c = rt->cpu(); auto& m = rt->memory();
            constexpr std::uint32_t code = 0x08804000, matrix = 0x08001000, depth = 0x08001100;
            constexpr std::array words{
                0xD8580000u, 0xD8590010u, 0xD85A0020u, 0xD85B0030u, // lv.q C600..C630
                0xCA200000u, 0xD0070020u, // lv.s S000; vone.s S010
                0x64FA00C0u, 0xD2004000u, 0x48630000u}; // vdot.p; vf2in.s; mfv
            for (unsigned i = 0; i < words.size(); ++i) m.store32(code + i * 4, words[i]);
            for (unsigned i = 0; i < 16; ++i) m.store32(matrix + i * 4, std::bit_cast<std::uint32_t>(999.0f));
            m.store32(matrix + 40, std::bit_cast<std::uint32_t>(sample.scale));
            m.store32(matrix + 56, std::bit_cast<std::uint32_t>(sample.offset));
            m.store32(depth, std::bit_cast<std::uint32_t>(sample.depth));
            c.eat_vfpu_prefixes();
            c.pc = code; c.gpr[2] = matrix; c.gpr[17] = depth;
            const auto result = psprecomp::interpret_allegrex(*rt, c, words.size());
            if (result != psprecomp::InterpreterExit::Budget || rt->stopped() ||
                c.pc != code + words.size() * 4 || c.gpr[3] != static_cast<std::uint32_t>(sample.expected))
                throw std::runtime_error("Affine depth lane mapping or ties-to-even conversion failed");
            if (c.vfpu_ctrl[0] != 0xE4 || c.vfpu_ctrl[1] != 0xE4 || c.vfpu_ctrl[2] != 0)
                throw std::runtime_error("Geometry sequence left pending VFPU prefixes");
        }
        std::cout << "Synthetic VFPU geometry lane/rounding contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
