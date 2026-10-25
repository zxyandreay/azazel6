#include "azazel6/ge_clear.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>
#include <vector>

namespace azazel6 {
std::uint64_t execute_ge_clear(psprecomp::GuestMemory& memory, const GeState& state,
    std::uint32_t command, std::uint32_t lease_begin, std::uint32_t lease_end) {
    const auto count = command & 0xFFFFu, vertex = state.vertex_address();
    const auto ram_end = psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(memory.size());
    if (command >> 16 != 0x0406u || !count || count > 64 || (count & 1u) ||
        state.word(0x12) != 0x80011Cu || state.word(0xD3) != 0x701u || state.word(0xD2) ||
        state.word(0xE7) || state.word(0xE8) || state.word(0xE9))
        throw psprecomp::Error("Unsupported GE clear primitive state");
    if (((vertex | lease_begin | lease_end) & 3u) || lease_begin < psprecomp::GuestMemory::kPhysicalBase ||
        lease_begin >= lease_end || lease_end > ram_end || vertex < lease_begin ||
        static_cast<std::uint64_t>(vertex) + count * 12 > lease_end)
        throw psprecomp::Error("GE clear vertices outside live command lease");
    const auto target = [&](unsigned base_op, unsigned stride_op) {
        const auto width = state.word(stride_op);
        if (width & ~0xFF07FFu) throw psprecomp::Error("Unsupported GE clear framebuffer width bits");
        auto address = state.word(base_op) | ((width & 0xFF0000u) << 8);
        if (address < 0x00200000u) address += psprecomp::GuestMemory::kVramPhysicalBase;
        const auto stride = width & 0x7FFu;
        if ((address & 15u) || !stride || stride > 1024 || address < psprecomp::GuestMemory::kVramPhysicalBase ||
            static_cast<std::uint64_t>(address) >= psprecomp::GuestMemory::kVramPhysicalBase +
                static_cast<std::uint64_t>(memory.vram_size()))
            throw psprecomp::Error("GE clear requires canonical owned VRAM targets");
        return std::pair{address, stride};
    };
    const auto [color_base, color_stride] = target(0x9C, 0x9D);
    const auto [depth_base, depth_stride] = target(0x9E, 0x9F);
    const auto scissor0 = state.word(0xD4), scissor1 = state.word(0xD5);
    const auto left = scissor0 & 1023u, top = (scissor0 >> 10) & 1023u;
    const auto right = (scissor1 & 1023u) + 1, bottom = ((scissor1 >> 10) & 1023u) + 1;
    if ((scissor0 | scissor1) & ~0xFFFFFu || left >= right || top >= bottom)
        throw psprecomp::Error("Unsupported GE clear scissor");
    struct Rect { std::uint32_t x0, y0, x1, y1; std::uint16_t color, depth; };
    std::vector<Rect> rectangles;
    std::uint64_t pixels = 0;
    for (std::uint32_t i = 0; i < count; i += 2) {
        const auto a = vertex + i * 12, b = a + 12;
        const auto x0 = memory.load16(a + 4), y0 = memory.load16(a + 6);
        const auto x1 = memory.load16(b + 4), y1 = memory.load16(b + 6);
        const auto color = memory.load32(b);
        const auto depth = memory.load16(b + 8);
        if (x0 >= x1 || y0 >= y1 || x1 > 1024 || y1 > 1024 ||
            memory.load32(a) != color || memory.load16(a + 8) != depth)
            throw psprecomp::Error("Unsupported GE clear rectangle geometry");
        Rect r{std::max<std::uint32_t>(x0, left), std::max<std::uint32_t>(y0, top),
            std::min<std::uint32_t>(x1, right), std::min<std::uint32_t>(y1, bottom),
            static_cast<std::uint16_t>(((color & 0xF8u) >> 3) | ((color & 0xFC00u) >> 5) |
                ((color & 0xF80000u) >> 8)), depth};
        if (r.x0 >= r.x1 || r.y0 >= r.y1) continue;
        pixels += static_cast<std::uint64_t>(r.x1 - r.x0) * (r.y1 - r.y0);
        if (pixels > 1048576) throw psprecomp::Error("GE clear pixel budget exhausted");
        for (const auto [base, stride] : {std::pair{color_base, color_stride}, std::pair{depth_base, depth_stride}}) {
            const auto end = static_cast<std::uint64_t>(base) +
                (static_cast<std::uint64_t>(r.y1 - 1) * stride + r.x1) * 2;
            if (r.x1 > stride || end > psprecomp::GuestMemory::kVramPhysicalBase +
                static_cast<std::uint64_t>(memory.vram_size()))
                throw psprecomp::Error("GE clear destination exceeds owned VRAM/stride");
        }
        rectangles.push_back(r);
    }
    // DOCUMENTED reference behavior: depth precedes color for each rectangle.
    // Preserve this order even when the two guest destinations alias.
    for (const auto& r : rectangles) {
        for (auto y = r.y0; y < r.y1; ++y) for (auto x = r.x0; x < r.x1; ++x)
            memory.store16(depth_base + (y * depth_stride + x) * 2, r.depth);
        for (auto y = r.y0; y < r.y1; ++y) for (auto x = r.x0; x < r.x1; ++x)
            memory.store16(color_base + (y * color_stride + x) * 2, r.color);
    }
    return pixels;
}
}
