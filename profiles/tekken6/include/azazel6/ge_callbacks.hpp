#pragma once
#include "psprecomp/runtime.hpp"
#include <array>
#include <memory>
#include <optional>

namespace azazel6 {
struct GeCallback {
    std::uint32_t signal_function, signal_argument, finish_function, finish_argument;
};
struct GeCallbacks {
    std::array<std::optional<GeCallback>, 16> slots{};
    [[nodiscard]] std::size_t count() const noexcept;
};
// Own registrations only; GE list execution and interrupt delivery remain unsupported.
std::shared_ptr<GeCallbacks> register_ge_callback_hle(psprecomp::Runtime&,
                                                    std::uint32_t code_begin, std::uint32_t code_end);
}
