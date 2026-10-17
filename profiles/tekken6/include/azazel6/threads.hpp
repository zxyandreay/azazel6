#pragma once
#include "psprecomp/runtime.hpp"
#include "azazel6/memory.hpp"
#include <memory>
#include <string>
#include <vector>

namespace azazel6 {
inline constexpr std::uint32_t kCreateThreadNid = 0x446D8DE6u;
inline constexpr std::uint32_t kDeleteThreadNid = 0x9FA03CD3u;
inline constexpr std::uint32_t kStartThreadNid = 0xF475845Du;
inline constexpr std::uint32_t kThreadReturnGate = 0x08000000u;
inline constexpr std::uint32_t kBootstrapUid = 0x7FFFFFFFu;

// Explicit ownership for the current single-module experiment, not a PSP partition allocator.
struct ThreadArena {
    std::uint32_t begin, end; // Exclusive end; excludes image and bootstrap stack.
    std::uint32_t image_begin, image_end;
    std::uint32_t executable_begin, executable_end;
    std::uint32_t module_gp;
    std::uint32_t bootstrap_priority = 0, bootstrap_stack_size = 0;
};
enum class ThreadStatus : std::uint32_t { Running = 1, Ready = 2, Dormant = 16 };
struct GuestThread {
    std::uint32_t uid, entry, priority, attributes, stack_base, stack_size, gp;
    std::string name;
    ThreadStatus status = ThreadStatus::Dormant;
    psprecomp::AllegrexContext context{};
    std::uint32_t exit_status = 0;
    std::uint64_t stack_ticket = 0;
};
class ThreadRegistry {
public:
    [[nodiscard]] std::vector<GuestThread> snapshot() const { return threads_; }
    [[nodiscard]] std::uint32_t current_uid() const { return current_uid_; }
private:
    friend std::shared_ptr<ThreadRegistry> register_thread_hle(psprecomp::Runtime&, ThreadArena, std::shared_ptr<GuestAllocator>);
    explicit ThreadRegistry(ThreadArena arena) : arena_(arena) {}
    void create(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void erase(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void start(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void finish(psprecomp::Runtime&, psprecomp::AllegrexContext&, bool remove, bool from_return);
    void schedule(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    GuestThread* find(std::uint32_t);
    void prepare(psprecomp::Runtime&, GuestThread&, const std::vector<std::uint8_t>&);
    ThreadArena arena_;
    std::shared_ptr<GuestAllocator> allocator_;
    std::vector<GuestThread> threads_;
    GuestThread bootstrap_{};
    std::uint32_t current_uid_ = 0;
    std::uint32_t next_uid_ = 1;
};
[[nodiscard]] std::shared_ptr<ThreadRegistry> register_thread_hle(psprecomp::Runtime&, ThreadArena,
                                                               std::shared_ptr<GuestAllocator> = {});
} // namespace azazel6
