#pragma once
#include "gpu/ge_state.hpp"
#include "psprecomp/common.hpp"
#include <cmath>

namespace azazel6 {
// The first observed PRIM uses unindexed through-mode RGBA8888/16-bit vertices.
// Reuse the pinned decoder only after the complete input is inside a live lease.
// Returning vertices is not rendering or permission to complete the GE command.
inline std::vector<portablekit::gpu::Vertex> decode_clear_vertices(
    const psprecomp::GuestMemory& memory, std::uint32_t address, std::uint32_t count,
    std::uint32_t lease_begin, std::uint32_t lease_end) {
    constexpr std::uint32_t type = 0x80011Cu, stride = 12;
    const auto ram_end = psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(memory.size());
    const auto end = static_cast<std::uint64_t>(address) + static_cast<std::uint64_t>(count) * stride;
    if (!count || count > 64 || (count & 1u) || ((address | lease_begin | lease_end) & 3u) ||
        lease_begin < psprecomp::GuestMemory::kPhysicalBase || lease_begin >= lease_end || lease_end > ram_end ||
        address < lease_begin || end > lease_end)
        throw psprecomp::Error("Clear vertices require aligned pairs inside one canonical live lease");
    std::vector<portablekit::gpu::Vertex> result;
    if (portablekit::gpu::decode_vertices(memory, address, type, count, result) != stride || result.size() != count)
        throw psprecomp::Error("PortableKit clear vertex layout mismatch");
    for (const auto& vertex : result) {
        const auto& p = vertex.position;
        if (!std::isfinite(p[0]) || !std::isfinite(p[1]) || !std::isfinite(p[2]) ||
            p[0] < 0 || p[0] > 1024 || p[1] < 0 || p[1] > 1024 || p[2] < 0 || p[2] > 65535)
            throw psprecomp::Error("Clear vertex outside reviewed through-mode domain");
    }
    for (std::size_t i = 0; i < result.size(); i += 2) {
        const auto& a = result[i]; const auto& b = result[i + 1];
        if (a.position[0] >= b.position[0] || a.position[1] >= b.position[1] ||
            a.position[2] != b.position[2] || a.color != b.color)
            throw psprecomp::Error("Clear pair requires an ordered rectangle and constant color/depth");
    }
    return result;
}
}
