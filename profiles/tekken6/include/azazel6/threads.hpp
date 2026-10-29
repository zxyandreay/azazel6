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
inline constexpr std::uint32_t kDisplayIdleGate = 0x08000008u;

// Explicit ownership for the current single-module experiment, not a PSP partition allocator.
struct ThreadArena {
    std::uint32_t begin, end; // Exclusive end; excludes image and bootstrap stack.
    std::uint32_t image_begin, image_end;
    std::uint32_t executable_begin, executable_end;
    std::uint32_t module_gp;
    std::uint32_t bootstrap_priority = 0, bootstrap_stack_size = 0;
};
enum class ThreadStatus : std::uint32_t { Running = 1, Ready = 2, Waiting = 4, Dormant = 16 };
struct GuestThread {
    std::uint32_t uid, entry, priority, attributes, stack_base, stack_size, gp;
    std::string name;
    ThreadStatus status = ThreadStatus::Dormant;
    psprecomp::AllegrexContext context{};
    std::uint32_t exit_status = 0;
    std::uint64_t stack_ticket = 0;
    std::optional<std::uint64_t> display_deadline;
    // Absolute half-ticks on the shared LCD timeline; independent of display frames.
    std::optional<std::uint64_t> timeout_deadline;
    std::uint32_t timeout_result = 0;
    std::optional<std::uint32_t> event_wait;
    std::optional<std::uint32_t> semaphore_wait;
};
struct SemaphoreWait { std::uint32_t thread, count; };
struct GuestMutex {
    std::string name;
    std::uint32_t attributes, initial_count, count, owner;
};
struct GuestLwMutex {
    std::string name;
    std::uint32_t workarea, attributes, initial_count, count, owner;
};
struct GuestSemaphore {
    std::string name;
    std::uint32_t attributes, initial_count, count, maximum;
    std::vector<SemaphoreWait> waiters;
};
// Only inactive timers are supported until a guest clock is established.
struct GuestVTimer { std::string name; };
struct GuestCallback { std::string name; std::uint32_t owner, function, common; };
// Startup currently requests one block per fixed pool; no waiting allocation.
struct GuestFixedPool {
    std::string name;
    GuestAllocation allocation;
    std::uint32_t block_size, alignment, attributes;
    bool allocated = false;
};
struct EventWait { std::uint32_t thread, bits, mode, output; };
struct GuestEventFlag {
    std::string name;
    std::uint32_t attributes, initial, pattern;
    std::vector<EventWait> waiters;
};
class ThreadRegistry {
public:
    [[nodiscard]] std::vector<GuestThread> snapshot() const { return threads_; }
    [[nodiscard]] std::optional<GuestThread> current_thread() const;
    [[nodiscard]] std::uint32_t current_uid() const { return current_uid_; }
    [[nodiscard]] bool interrupts_enabled() const { return interrupts_enabled_; }
    [[nodiscard]] bool in_interrupt() const { return in_interrupt_; }
    [[nodiscard]] bool can_idle_display() const;
    [[nodiscard]] std::optional<std::uint64_t> next_timeout() const;
    bool wait_timeout(psprecomp::Runtime&, psprecomp::AllegrexContext&, std::uint64_t deadline, std::uint32_t result);
    void wake_timeouts(psprecomp::Runtime&, psprecomp::AllegrexContext&, std::uint64_t ticks);
    void wake_clock(psprecomp::Runtime&, psprecomp::AllegrexContext&, std::uint64_t ticks,
                    std::optional<std::uint64_t> display_edge);
    bool enter_interrupt();
    void leave_interrupt();
    bool wait_display(psprecomp::Runtime&, psprecomp::AllegrexContext&, std::uint64_t edge);
    void wake_display(psprecomp::Runtime&, psprecomp::AllegrexContext&, std::uint64_t edge);
    bool cancel_display(psprecomp::Runtime&, psprecomp::AllegrexContext&, std::uint32_t uid);
    void reschedule(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) { if (!in_interrupt_) schedule(rt, c); }
    [[nodiscard]] const std::map<std::uint32_t, GuestMutex>& mutexes() const { return mutexes_; }
    [[nodiscard]] const std::map<std::uint32_t, GuestLwMutex>& lw_mutexes() const { return lw_mutexes_; }
    [[nodiscard]] const std::map<std::uint32_t, GuestSemaphore>& semaphores() const { return semaphores_; }
    [[nodiscard]] const std::map<std::uint32_t, GuestVTimer>& timers() const { return timers_; }
    [[nodiscard]] const std::map<std::uint32_t, GuestCallback>& callbacks() const { return callbacks_; }
    [[nodiscard]] std::uint32_t exit_callback() const { return exit_callback_; }
    [[nodiscard]] const std::map<std::uint32_t, GuestFixedPool>& fixed_pools() const { return fixed_pools_; }
    [[nodiscard]] const std::map<std::uint32_t, GuestEventFlag>& event_flags() const { return event_flags_; }
private:
    friend std::shared_ptr<ThreadRegistry> register_thread_hle(psprecomp::Runtime&, ThreadArena, std::shared_ptr<GuestAllocator>);
    explicit ThreadRegistry(ThreadArena arena) : arena_(arena) {}
    void create(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void erase(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void start(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void change_priority(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void finish(psprecomp::Runtime&, psprecomp::AllegrexContext&, bool remove, bool from_return);
    void schedule(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    GuestThread* find(std::uint32_t);
    void prepare(psprecomp::Runtime&, GuestThread&, const std::vector<std::uint8_t>&);
    bool mutex_context(psprecomp::Runtime&);
    void create_mutex(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void create_lw_mutex(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void delete_mutex(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void lock_mutex(psprecomp::Runtime&, psprecomp::AllegrexContext&, bool try_only);
    void unlock_mutex(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void create_semaphore(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void wait_semaphore(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void create_timer(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void create_callback(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    bool callback_context(psprecomp::Runtime&);
    bool pool_context(psprecomp::Runtime&);
    void create_pool(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void delete_pool(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void allocate_pool(psprecomp::Runtime&, psprecomp::AllegrexContext&, bool try_only);
    void free_pool(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    bool event_context(psprecomp::Runtime&);
    void create_event(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void delete_event(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    void update_event(psprecomp::Runtime&, psprecomp::AllegrexContext&, bool clear);
    void poll_event(psprecomp::Runtime&, psprecomp::AllegrexContext&, bool try_only);
    void cancel_event(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    bool wake_event(psprecomp::Runtime&, std::uint32_t uid, GuestEventFlag&,
                    std::uint32_t pattern, std::uint32_t result);
    bool event_output(psprecomp::Runtime&, std::uint32_t address) const;
    ThreadArena arena_;
    std::shared_ptr<GuestAllocator> allocator_;
    std::vector<GuestThread> threads_;
    GuestThread bootstrap_{};
    std::uint32_t current_uid_ = 0;
    std::uint32_t next_uid_ = 1;
    bool interrupts_enabled_ = true;
    bool in_interrupt_ = false;
    std::map<std::uint32_t, GuestMutex> mutexes_;
    std::uint32_t next_mutex_uid_ = 0x30000000u;
    std::map<std::uint32_t, GuestLwMutex> lw_mutexes_;
    std::uint32_t next_lw_mutex_uid_ = 0x72000000u;
    std::map<std::uint32_t, GuestSemaphore> semaphores_;
    std::uint32_t next_semaphore_uid_ = 0x71000000u;
    std::map<std::uint32_t, GuestVTimer> timers_;
    std::uint32_t next_timer_uid_ = 0x40000000u;
    std::map<std::uint32_t, GuestCallback> callbacks_;
    std::uint32_t next_callback_uid_ = 0x50000000u, exit_callback_ = 0;
    std::map<std::uint32_t, GuestFixedPool> fixed_pools_;
    std::uint32_t next_pool_uid_ = 0x60000000u;
    std::map<std::uint32_t, GuestEventFlag> event_flags_;
    std::uint32_t next_event_uid_ = 0x70000000u;
};
[[nodiscard]] std::shared_ptr<ThreadRegistry> register_thread_hle(psprecomp::Runtime&, ThreadArena,
                                                               std::shared_ptr<GuestAllocator> = {});
} // namespace azazel6
