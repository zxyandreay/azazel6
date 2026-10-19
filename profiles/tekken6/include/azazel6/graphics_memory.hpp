#pragma once
#include "psprecomp/runtime.hpp"
#include <memory>

namespace azazel6 {
struct GraphicsMemoryQueries { std::uint64_t size_reads = 0, address_reads = 0; };
// Only immutable EDRAM geometry queries. No GE execution or display timing.
std::shared_ptr<GraphicsMemoryQueries> register_graphics_memory_hle(psprecomp::Runtime&);
}
