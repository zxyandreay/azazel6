#pragma once
#include "azazel6/ge_callbacks.hpp"
#include "azazel6/interrupts.hpp"
#include "azazel6/ge_state.hpp"
#include <deque>
#include <map>
#include <vector>

namespace azazel6 {
enum class GeStatus { Queued, Stalled, Callback, Complete, Failed };
struct GeCallbackEvent {
    std::uint64_t token;
    std::uint32_t list, function, argument, value;
    bool finish;
};
struct GeList {
    std::uint32_t begin, end, pc, stall;
    std::optional<std::uint32_t> callback;
    std::uint64_t callback_generation = 0;
    GeStatus status = GeStatus::Queued;
    std::uint32_t commands = 0;
    std::vector<std::uint32_t> returns;
    std::optional<std::uint32_t> terminal;
    std::optional<GeCallbackEvent> event;
    bool event_taken = false;
    std::string error;
};
// Independently authored control-stream model with a bounded clear-sprite subset.
// Spans are explicit canonical-RAM leases supplied by the host.
class GeQueue {
public:
    GeQueue(psprecomp::GuestMemory& memory, std::shared_ptr<GeCallbacks> callbacks);
    std::uint32_t enqueue(std::uint32_t begin, std::uint32_t end, std::uint32_t stall = 0,
                          std::optional<std::uint32_t> callback = {});
    void update_stall(std::uint32_t id, std::uint32_t stall);
    void erase(std::uint32_t id);
    [[nodiscard]] const GeList& list(std::uint32_t id) const;
    [[nodiscard]] const GeState& state() const { return state_; }
    [[nodiscard]] std::size_t size() const { return lists_.size(); }
    [[nodiscard]] const auto& lists() const { return lists_; }
    [[nodiscard]] std::uint64_t clear_draws() const { return clear_draws_; }
    [[nodiscard]] std::uint64_t clear_pixels() const { return clear_pixels_; }
    [[nodiscard]] bool all_complete() const;
    [[nodiscard]] bool owns_memory(const psprecomp::GuestMemory& memory) const { return &memory_ == &memory; }
    // FIFO, one head per call. Per-call and total-list budgets are mandatory.
    void run(std::uint32_t budget);
    std::optional<GeCallbackEvent> take_callback();
    void complete_callback(std::uint64_t token);
private:
    bool callback_live(const GeList&) const;
    void fail(GeList&, std::string why);
    void check_word(const GeList&, std::uint32_t address) const;
    void terminal(GeList&, std::uint32_t id);
    psprecomp::GuestMemory& memory_;
    std::shared_ptr<GeCallbacks> callbacks_;
    std::map<std::uint32_t, GeList> lists_;
    std::deque<std::uint32_t> queue_;
    std::uint32_t next_id_ = 1;
    std::uint64_t next_event_ = 1;
    GeState state_;
    std::uint64_t clear_draws_ = 0, clear_pixels_ = 0;
};
struct GeSpan { std::uint32_t begin, end; };
struct GeSubmissionSource {
    std::shared_ptr<const PartitionMemory> partition;
    std::shared_ptr<const DataCacheState> cache;
    std::shared_ptr<InterruptDelivery> interrupts;
    std::uint32_t gp = 0;
};
// Bounded synchronous streams: configured initialization or freshly written live
// partition spans. Optional FINISH is acknowledged only after the guest returns.
std::shared_ptr<GeQueue> register_ge_list_hle(psprecomp::Runtime&, std::shared_ptr<GeCallbacks>,
    std::shared_ptr<ThreadRegistry>, std::vector<GeSpan>, GeSubmissionSource = {});
// One callback at an outer boundary. Completion is acknowledged only by the guarded guest return.
bool dispatch_ge_callback(psprecomp::Runtime&, const std::shared_ptr<GeQueue>&,
                          InterruptDelivery&, std::uint32_t module_gp);
}
