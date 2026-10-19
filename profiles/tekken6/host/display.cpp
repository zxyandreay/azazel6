#include "azazel6/display.hpp"
#include "psprecomp/common.hpp"
#include <limits>

namespace azazel6 {
std::vector<std::uint64_t> DisplayClock::advance(std::uint64_t delta) {
    if (delta > std::numeric_limits<std::uint64_t>::max() - ticks_)
        throw psprecomp::Error("Display clock overflow");
    const auto next = ticks_ + delta;
    const auto count = next / ticks_per_frame - frames();
    if (count > max_events) throw psprecomp::Error("Display event budget exhausted");
    std::vector<std::uint64_t> edges;
    edges.reserve(static_cast<std::size_t>(count));
    for (auto i = frames(); i < next / ticks_per_frame; ++i)
        edges.push_back((i + 1) * ticks_per_frame);
    ticks_ = next;
    return edges;
}
namespace {
void check_range(const psprecomp::GuestMemory& mem, Framebuffer f) {
    if (!f.address) return; // Display off owns no guest memory.
    const auto bytes = static_cast<std::uint64_t>(f.stride) * 272 * (f.format == 3 ? 4 : 2);
    const auto end = static_cast<std::uint64_t>(f.address) + bytes;
    const bool ram = f.address >= psprecomp::GuestMemory::kPhysicalBase &&
        end <= psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(mem.size());
    const bool vram = f.address >= psprecomp::GuestMemory::kVramPhysicalBase &&
        end <= psprecomp::GuestMemory::kVramPhysicalBase + static_cast<std::uint64_t>(mem.vram_size());
    if (!ram && !vram) throw psprecomp::Error("Framebuffer outside bounded canonical RAM/VRAM");
    if (f.stride < 480) throw psprecomp::Error("Framebuffer stride below stock LCD width is unsupported");
}
}
std::uint32_t DisplayBuffers::set(const psprecomp::GuestMemory& mem, Framebuffer f, std::uint32_t sync) {
    if (sync > 1) return 0x80000107u;
    if (f.address & 15u) return 0x80000103u;
    if ((f.stride & 63u) || (!f.stride && f.address)) return 0x80000104u;
    if (f.format > 3) return 0x80000108u;
    if (!sync && (f.stride != latched_.stride || f.format != latched_.format)) return 0x80000107u;
    check_range(mem, f);
    // Next-frame updates change current format/stride before its base latches.
    // Validate that intermediate descriptor as well, before changing either state.
    auto intermediate = current_;
    intermediate.stride = f.stride; intermediate.format = f.format;
    if (sync) check_range(mem, intermediate);
    latched_ = f;
    if (sync) { current_ = intermediate; pending_ = true; }
    else { current_ = f; pending_ = false; }
    return 0;
}
void DisplayBuffers::vblank() noexcept {
    if (pending_) { current_ = latched_; pending_ = false; }
}
}
