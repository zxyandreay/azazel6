#include "azazel6/threads.hpp"
#include <algorithm>

namespace azazel6 {
namespace {
constexpr std::uint32_t no_memory = 0x80020190u, not_found = 0x8002019Du;
bool canonical(const psprecomp::GuestMemory& memory, std::uint32_t address, std::uint32_t size) {
    return address >= psprecomp::GuestMemory::kPhysicalBase &&
        static_cast<std::uint64_t>(address) + size <=
            psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(memory.size());
}
}
bool ThreadRegistry::pool_context(psprecomp::Runtime& rt) {
    const auto* current = find(current_uid_);
    if (current && current->status == ThreadStatus::Running && interrupts_enabled_) return true;
    rt.stop("Fixed pool operation requires a running thread with interrupts enabled"); return false;
}
void ThreadRegistry::create_pool(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!pool_context(rt)) return;
    const auto name_address = c.gpr[4], partition = c.gpr[5], attr = c.gpr[6];
    const auto size = c.gpr[7], count = c.gpr[8], options = c.gpr[9];
    if (!name_address) { c.set_gpr(2, no_memory); return; }
    if (partition != 2) { rt.stop("Fixed pool requires verified user partition 2"); return; }
    if (attr & ~0x41FFu) { c.set_gpr(2, 0x80020191u); return; }
    if (attr & ~0x4100u) { rt.stop("Unsupported fixed pool attribute semantics"); return; }
    if (!size || size > 0x7FFFFFFFu || !count || count > 0x7FFFFFFFu) {
        c.set_gpr(2, 0x800201B7u); return;
    }
    if (count != 1) { rt.stop("Multiple-block fixed pool allocation order is unimplemented"); return; }
    std::uint32_t alignment = 8;
    if (options) {
        if ((options & 3u) || !canonical(rt.memory(), options, 4)) {
            rt.stop("Unsupported fixed pool options address"); return;
        }
        const auto option_size = rt.memory().load32(options);
        if (option_size != 4 && option_size != 8) {
            rt.stop("Unsupported fixed pool options layout"); return;
        }
        if (option_size == 8) {
            if (!canonical(rt.memory(), options, 8)) { rt.stop("Truncated fixed pool options"); return; }
            alignment = rt.memory().load32(options + 4);
            if (alignment && (alignment & (alignment - 1))) { c.set_gpr(2, 0x800200D2u); return; }
            alignment = std::max(alignment, 8u);
            if (alignment > 4096) { rt.stop("Fixed pool alignment exceeds verified range"); return; }
        }
    }
    std::string name;
    for (std::uint32_t i = 0; i < 31; ++i) {
        const auto address = static_cast<std::uint64_t>(name_address) + i;
        if (address > 0xFFFFFFFFull || !canonical(rt.memory(), static_cast<std::uint32_t>(address), 1)) {
            rt.stop("Unsupported fixed pool name address"); return;
        }
        const auto byte = rt.memory().load8(static_cast<std::uint32_t>(address));
        if (!byte) break;
        name.push_back(static_cast<char>(byte));
    }
    if (fixed_pools_.size() >= 4096 || next_pool_uid_ >= 0x70000000u) { c.set_gpr(2, no_memory); return; }
    const auto allocation = allocator_->allocate(size, alignment, (attr & 0x4000u) != 0);
    if (!allocation) { c.set_gpr(2, no_memory); return; }
    try {
        fixed_pools_.emplace(next_pool_uid_, GuestFixedPool{std::move(name), *allocation, size, alignment, attr});
    } catch (...) { allocator_->release(allocation->ticket); throw; }
    c.set_gpr(2, next_pool_uid_++);
}
void ThreadRegistry::delete_pool(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!pool_context(rt)) return;
    const auto found = fixed_pools_.find(c.gpr[4]);
    if (found == fixed_pools_.end()) { c.set_gpr(2, not_found); return; }
    // There can be no waiters: blocking exhaustion never mutates guest state.
    allocator_->release(found->second.allocation.ticket);
    fixed_pools_.erase(found); c.set_gpr(2, 0);
}
void ThreadRegistry::allocate_pool(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c, bool try_only) {
    if (!pool_context(rt)) return;
    const auto found = fixed_pools_.find(c.gpr[4]);
    if (found == fixed_pools_.end()) { c.set_gpr(2, not_found); return; }
    if (!try_only && c.gpr[6]) { rt.stop("Fixed pool allocation timeouts are unimplemented"); return; }
    const auto output = c.gpr[5];
    // Restrict outputs to canonical writable startup data, never loaded code.
    if ((output & 3u) || output < arena_.executable_end || !canonical(rt.memory(), output, 4) ||
        rt.has_function(output)) {
        rt.stop("Unsupported fixed pool output address"); return;
    }
    auto& pool = found->second;
    if (pool.allocated) {
        if (try_only) c.set_gpr(2, no_memory);
        else rt.stop("Fixed pool allocation requires a verified waiting scheduler");
        return;
    }
    rt.memory().store32(output, pool.allocation.base);
    pool.allocated = true; c.set_gpr(2, 0);
}
void ThreadRegistry::free_pool(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!pool_context(rt)) return;
    const auto found = fixed_pools_.find(c.gpr[4]);
    if (found == fixed_pools_.end()) { c.set_gpr(2, not_found); return; }
    auto& pool = found->second;
    if (c.gpr[5] != pool.allocation.base || !pool.allocated) {
        if (c.gpr[5] && !canonical(rt.memory(), c.gpr[5], 1)) {
            rt.stop("Unsupported fixed pool free address"); return;
        }
        c.set_gpr(2, 0x800201B6u); return;
    }
    pool.allocated = false; c.set_gpr(2, 0);
}
} // namespace azazel6
