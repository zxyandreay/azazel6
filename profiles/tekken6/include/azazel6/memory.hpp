#pragma once
#include "psprecomp/runtime.hpp"
#include <map>
#include <memory>
#include <optional>
#include <vector>

namespace azazel6 {
struct GuestAllocation { std::uint64_t ticket; std::uint32_t base, size; };
class GuestAllocator {
public:
    GuestAllocator(std::uint32_t begin, std::uint32_t end);
    std::optional<GuestAllocation> allocate(std::uint32_t size, std::uint32_t alignment = 256,
                                           bool high = false, std::optional<std::uint32_t> fixed = {});
    bool release(std::uint64_t ticket);
    std::uint32_t total_free() const;
    std::uint32_t max_free() const;
    std::uint32_t begin() const { return begin_; }
    std::uint32_t end() const { return end_; }
private:
    std::vector<std::pair<std::uint32_t, std::uint32_t>> holes() const;
    std::uint32_t begin_, end_;
    std::uint64_t next_ticket_ = 1;
    std::map<std::uint64_t, GuestAllocation> allocations_;
};
struct PartitionMemory {
    std::shared_ptr<GuestAllocator> allocator;
    std::map<std::uint32_t, GuestAllocation> blocks;
    std::uint32_t next_uid = 0x10000000u;
};
std::shared_ptr<PartitionMemory> register_memory_hle(psprecomp::Runtime&, std::shared_ptr<GuestAllocator>);
struct DataCacheState {
    std::uint64_t completed_ranges = 0;
    std::uint32_t last_address = 0, last_size = 0;
};
// Single-threaded coherent RAM only, outside the file-backed executable image.
std::shared_ptr<DataCacheState> register_data_cache_hle(psprecomp::Runtime&,
                                                     std::uint32_t data_begin, std::uint32_t data_end);
} // namespace azazel6
