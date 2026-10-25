#pragma once
#include "azazel6/ge_state.hpp"
#include "psprecomp/guest_memory.hpp"

namespace azazel6 {
// Original bounded clear rasterizer for the observed through-mode sprite format.
// Preflights every source/destination before writing. Other draw states reject.
std::uint64_t execute_ge_clear(psprecomp::GuestMemory&, const GeState&,
    std::uint32_t command, std::uint32_t lease_begin, std::uint32_t lease_end);
}
