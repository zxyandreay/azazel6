#include "azazel6/memory.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>

namespace azazel6 {
std::shared_ptr<DataCacheState> register_data_cache_hle(psprecomp::Runtime& rt,
    std::uint32_t begin, std::uint32_t end,
    std::vector<std::pair<std::uint32_t, std::uint32_t>> writeback_spans,
    std::shared_ptr<const PartitionMemory> partition) {
    if (begin < psprecomp::GuestMemory::kPhysicalBase || begin >= end || ((begin | end) & 63u) ||
        static_cast<std::uint64_t>(end) > psprecomp::GuestMemory::kPhysicalBase +
            static_cast<std::uint64_t>(rt.memory().size()))
        throw psprecomp::Error("Invalid coherent data-cache range");
    if (partition && (partition->owner != &rt || !partition->allocator ||
        partition->allocator->begin() < begin || partition->allocator->end() > end))
        throw psprecomp::Error("Invalid cache partition ownership");
    if (writeback_spans.size() > 64) throw psprecomp::Error("Too many cache writeback spans");
    std::sort(writeback_spans.begin(), writeback_spans.end());
    std::uint32_t previous = psprecomp::GuestMemory::kPhysicalBase;
    for (auto [lo, hi] : writeback_spans) {
        if (lo < previous || lo >= hi || hi > begin || hi - lo > 16384u || ((lo | hi) & 63u))
            throw psprecomp::Error("Invalid cache writeback data span");
        previous = hi;
    }
    auto state = std::make_shared<DataCacheState>();
    state->owner = &rt;
    const auto operation = [state, begin, end, writeback_spans, partition](auto& runtime, auto& c, bool writeback) {
        const auto address = c.gpr[4], size = c.gpr[5];
        const auto finish = static_cast<std::uint64_t>(address) + size;
        const bool extra = writeback && std::any_of(writeback_spans.begin(), writeback_spans.end(),
            [address, finish](auto span) { return address >= span.first && finish <= span.second; });
        const bool large_owned = writeback && partition && size <= 0x80000u &&
            std::any_of(partition->blocks.begin(), partition->blocks.end(),
                [address, finish](const auto& item) {
                    const auto& block = item.second;
                    return address >= block.base &&
                        finish <= static_cast<std::uint64_t>(block.base) + block.size;
                });
        if ((size >= 0x10000u && !large_owned) || (!(address >= begin && finish <= end) && !extra)) {
            runtime.stop("Data-cache range outside verified coherent startup memory"); return;
        }
        if (size) {
            const auto line_end = (finish + 63u) & ~std::uint64_t{63u};
            for (auto pc = static_cast<std::uint64_t>(address & ~63u); pc < line_end; pc += 4) {
                if (runtime.has_function(static_cast<std::uint32_t>(pc))) {
                    runtime.stop("Data-cache range intersects registered guest code"); return;
                }
            }
        }
        // AOT stores and HLE accesses share the same RAM backing. There is no
        // separate CPU data cache or asynchronous device copy in this probe.
        ++state->completed_ranges;
        if (writeback) ++state->writeback_ranges;
        state->last_address = address; state->last_size = size;
        state->writeback_lease.reset();
        if (writeback && size && partition) {
            for (const auto& [uid, block] : partition->blocks) {
                if (address >= block.base && finish <= static_cast<std::uint64_t>(block.base) + block.size) {
                    state->writeback_lease = GuestAllocation{block.ticket, address, size};
                    break;
                }
            }
        }
        c.set_gpr(2, 0); // Inspected successful L1 range paths finish with v0 zero.
    };
    rt.register_hle("UtilsForUser", 0x34B9FA9Eu, [operation](auto& runtime, auto& c) { operation(runtime, c, false); });
    rt.register_hle("UtilsForUser", 0x3EE30821u, [operation](auto& runtime, auto& c) { operation(runtime, c, true); });
    return state;
}

GuestAllocator::GuestAllocator(std::uint32_t begin, std::uint32_t end) : begin_(begin), end_(end) {
    if (begin < 0x08000000u || end > 0x0C000000u || begin >= end || ((begin | end) & 255u))
        throw psprecomp::Error("Invalid canonical guest allocation arena");
}
std::vector<std::pair<std::uint32_t, std::uint32_t>> GuestAllocator::holes() const {
    std::vector<std::pair<std::uint32_t, std::uint32_t>> used, free;
    for (const auto& [ticket, a] : allocations_) used.emplace_back(a.base, a.base + a.size);
    std::sort(used.begin(), used.end());
    auto cursor = begin_;
    for (const auto& [base, end] : used) { if (cursor < base) free.emplace_back(cursor, base); cursor = end; }
    if (cursor < end_) free.emplace_back(cursor, end_);
    return free;
}
std::optional<GuestAllocation> GuestAllocator::allocate(std::uint32_t size, std::uint32_t alignment,
                                                      bool high, std::optional<std::uint32_t> fixed) {
    if (!size || !alignment || (alignment & (alignment - 1))) return {};
    alignment = std::max(alignment, 256u);
    const std::uint64_t rounded = (static_cast<std::uint64_t>(size) + 255u) & ~std::uint64_t{255u};
    if (rounded > end_ - begin_ || (fixed && (*fixed & 255u))) return {};
    auto free = holes();
    if (high) std::reverse(free.begin(), free.end());
    for (const auto& [lo, hi] : free) {
        if (rounded > hi - lo) continue;
        const std::uint64_t candidate = fixed ? *fixed : high ?
            (static_cast<std::uint64_t>(hi) - rounded) & ~(static_cast<std::uint64_t>(alignment) - 1) :
            (static_cast<std::uint64_t>(lo) + alignment - 1) & ~(static_cast<std::uint64_t>(alignment) - 1);
        if (candidate < lo || candidate + rounded > hi) continue;
        GuestAllocation result{next_ticket_, static_cast<std::uint32_t>(candidate), static_cast<std::uint32_t>(rounded)};
        allocations_.emplace(next_ticket_, result);
        ++next_ticket_;
        return result;
    }
    return {};
}
bool GuestAllocator::release(std::uint64_t ticket) { return allocations_.erase(ticket) != 0; }
std::uint32_t GuestAllocator::total_free() const {
    std::uint32_t sum = 0; for (auto [lo, hi] : holes()) sum += hi - lo; return sum;
}
std::uint32_t GuestAllocator::max_free() const {
    std::uint32_t best = 0; for (auto [lo, hi] : holes()) best = std::max(best, hi - lo); return best;
}

std::shared_ptr<PartitionMemory> register_memory_hle(psprecomp::Runtime& rt, std::shared_ptr<GuestAllocator> allocator) {
    if (!allocator || static_cast<std::uint64_t>(allocator->end()) > 0x08000000ull + rt.memory().size())
        throw psprecomp::Error("Partition arena outside RAM");
    auto state = std::make_shared<PartitionMemory>(); state->owner = &rt;
    state->allocator = std::move(allocator);
    rt.register_hle("SysMemUserForUser", 0x237DBD4Fu, [state](auto& runtime, auto& c) {
        if (c.gpr[4] != 2) { runtime.stop("Unsupported memory partition"); return; }
        const auto type = c.gpr[6], size = c.gpr[7], address = c.gpr[8];
        if (type > 4) { c.set_gpr(2, 0x800200D8u); return; }
        if ((type == 3 || type == 4) && (!address || (address & (address - 1)))) {
            c.set_gpr(2, 0x800200E4u); return;
        }
        if (!c.gpr[5]) { c.set_gpr(2, 0x80020001u); return; }
        // Bounded validation of the guest name; no pointer is retained by the host.
        for (std::uint32_t i = 0; i < 31; ++i) {
            const auto a = static_cast<std::uint64_t>(c.gpr[5]) + i;
            if (a < 0x08000000u || a >= 0x08000000ull + runtime.memory().size()) {
                c.set_gpr(2, 0x800200D3u); return;
            }
            if (!runtime.memory().load8(static_cast<std::uint32_t>(a))) break;
        }
        std::uint64_t adjusted = size;
        if (type == 2) adjusted += address & 255u;
        if (!size || adjusted > 0xFFFFFFFFull || state->next_uid >= 0x20000000u) {
            c.set_gpr(2, 0x800200D9u); return;
        }
        auto block = state->allocator->allocate(static_cast<std::uint32_t>(adjusted), type >= 3 ? address : 256,
            type == 1 || type == 4, type == 2 ? std::optional{address & ~255u} : std::nullopt);
        if (!block) { c.set_gpr(2, 0x800200D9u); return; }
        try { state->blocks.emplace(state->next_uid, *block); }
        catch (...) { state->allocator->release(block->ticket); throw; }
        c.set_gpr(2, state->next_uid++);
    });
    rt.register_hle("SysMemUserForUser", 0x9D9A5BA1u, [state](auto&, auto& c) {
        const auto found = state->blocks.find(c.gpr[4]);
        c.set_gpr(2, found == state->blocks.end() ? 0 : found->second.base);
    });
    rt.register_hle("SysMemUserForUser", 0xB6D61D02u, [state](auto&, auto& c) {
        const auto found = state->blocks.find(c.gpr[4]);
        if (found == state->blocks.end()) { c.set_gpr(2, 0x800200CBu); return; }
        state->allocator->release(found->second.ticket); state->blocks.erase(found); c.set_gpr(2, 0);
    });
    rt.register_hle("SysMemUserForUser", 0xA291F107u, [state](auto&, auto& c) { c.set_gpr(2, state->allocator->max_free()); });
    rt.register_hle("SysMemUserForUser", 0xF919F628u, [state](auto&, auto& c) { c.set_gpr(2, state->allocator->total_free()); });
    return state;
}
} // namespace azazel6
