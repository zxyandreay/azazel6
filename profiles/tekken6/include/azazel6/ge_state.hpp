#pragma once
#include <array>
#include <cstdint>

namespace azazel6 {
// Retained GE register state for observed initialization commands. No drawing,
// texture fetch, transfer, cache operation or matrix wrapping is implied.
class GeState {
public:
    bool write(std::uint32_t command);
    [[nodiscard]] std::uint32_t word(std::uint8_t opcode) const { return words_[opcode]; }
    [[nodiscard]] const auto& view_matrix_bits() const { return view_; }
    [[nodiscard]] std::uint32_t vertex_address() const { return vertex_; }
    [[nodiscard]] std::uint32_t index_address() const { return index_; }
    [[nodiscard]] std::uint64_t writes() const { return writes_; }
private:
    std::array<std::uint32_t, 256> words_{};
    std::array<std::uint32_t, 12> view_{};
    std::uint32_t view_cursor_ = 0, vertex_ = 0, index_ = 0;
    std::uint64_t writes_ = 0;
};
}
