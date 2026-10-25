#include "azazel6/ge_state.hpp"
#include "psprecomp/common.hpp"

namespace azazel6 {
void GeState::advance_vertices(std::uint32_t bytes) {
    if (static_cast<std::uint64_t>(vertex_) + bytes > 0x0FFFFFFFu)
        throw psprecomp::Error("GE vertex pointer overflow");
    vertex_ += bytes;
}
bool GeState::write(std::uint32_t command) {
    const auto op = command >> 24, value = command & 0xFFFFFFu;
    if (op == 1 || op == 2) {
        const auto relative = static_cast<std::uint64_t>((words_[0x10] & 0xF0000u) << 8 | value) +
            (static_cast<std::uint64_t>(words_[0x13]) << 8);
        if (relative > 0x0FFFFFFFu) throw psprecomp::Error("GE relative address exceeds supported physical range");
        (op == 1 ? vertex_ : index_) = static_cast<std::uint32_t>(relative);
    } else if (op == 0x3C) {
        if (value >= view_.size()) throw psprecomp::Error("Unsupported GE view matrix index");
        view_cursor_ = value;
    } else if (op == 0x3D) {
        if (view_cursor_ >= view_.size()) throw psprecomp::Error("GE view matrix write exceeds owned elements");
        view_[view_cursor_++] = value << 8; // GE float24 -> IEEE float32 bits, without host conversion.
    } else if (op == 0x10) {
        if (value & ~0xF0000u) throw psprecomp::Error("Unsupported GE BASE bits");
    } else if (!(op == 0x12 || op == 0x13 ||
        (op >= 0x15 && op <= 0x28) || (op >= 0x2C && op <= 0x33) || (op >= 0x36 && op <= 0x38) ||
        (op >= 0x42 && op <= 0x4D) || op == 0x50 || op == 0x51 ||
        (op >= 0x53 && op <= 0x58) || (op >= 0x5B && op <= 0xB5) ||
        (op >= 0xB8 && op <= 0xC3) ||
        (op >= 0xC5 && op <= 0xCA) || (op >= 0xCD && op <= 0xD0) ||
        (op >= 0xD2 && op <= 0xE9) || op == 0xEB || op == 0xEC || op == 0xEE)) {
        return false;
    }
    words_[op] = value;
    ++writes_;
    return true;
}
}
