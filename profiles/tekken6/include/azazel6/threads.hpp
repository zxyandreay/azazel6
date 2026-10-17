#pragma once
#include "psprecomp/runtime.hpp"
#include <memory>
#include <string>
#include <vector>

namespace azazel6 {
inline constexpr std::uint32_t kCreateThreadNid = 0x446D8DE6u;
inline constexpr std::uint32_t kDeleteThreadNid = 0x9FA03CD3u;

// Explicit ownership for the current single-module experiment, not a PSP partition allocator.
struct ThreadArena {
    std::uint32_t begin, end; // Exclusive end; excludes image and bootstrap stack.
    std::uint32_t image_begin, image_end;
    std::uint32_t executable_begin, executable_end;
    std::uint32_t module_gp;
};
struct DormantThread {
    std::uint32_t uid, entry, priority, attributes, stack_base, stack_size, gp;
    std::string name;
};
class ThreadRegistry {
public:
    [[nodiscard]] std::vector<DormantThread> snapshot() const { return threads_; }
private:
    friend std::shared_ptr<ThreadRegistry> register_thread_hle(psprecomp::Runtime&, ThreadArena);
    explicit ThreadRegistry(ThreadArena arena) : arena_(arena) {}
    void create(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void erase(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    ThreadArena arena_;
    std::vector<DormantThread> threads_;
    std::uint32_t next_uid_ = 1;
};
[[nodiscard]] std::shared_ptr<ThreadRegistry> register_thread_hle(psprecomp::Runtime&, ThreadArena);
} // namespace azazel6
