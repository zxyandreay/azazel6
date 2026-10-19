#pragma once
#include "psprecomp/runtime.hpp"
#include <memory>
#include <optional>
#include <deque>
#include "azazel6/threads.hpp"

namespace azazel6 {
struct VblankHandler {
    std::uint32_t function, argument, gp;
};
// One module owns the observed VBLANK slot. Registration never executes code.
struct InterruptState {
    std::optional<VblankHandler> vblank;
    bool enabled = false;
    std::uint64_t generation = 0;
};
std::shared_ptr<InterruptState> register_interrupt_hle(psprecomp::Runtime&,
    std::uint32_t code_begin, std::uint32_t code_end, std::uint32_t module_gp);
inline constexpr std::uint32_t kInterruptReturnGate = 0x08000004u;
class InterruptDelivery {
public:
    ~InterruptDelivery();
    // All calls occur at explicit outer-dispatch boundaries, never inside generated chains.
    void raise_vblank();
    bool dispatch(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    [[nodiscard]] bool active() const noexcept { return saved_.has_value(); }
    [[nodiscard]] std::size_t pending() const noexcept { return pending_.size(); }
private:
    friend std::shared_ptr<InterruptDelivery> register_interrupt_delivery(psprecomp::Runtime&,
        std::shared_ptr<InterruptState>, std::shared_ptr<ThreadRegistry>, std::shared_ptr<GuestAllocator>);
    void finish(psprecomp::Runtime&, psprecomp::AllegrexContext&);
    std::shared_ptr<InterruptState> state_;
    std::shared_ptr<ThreadRegistry> threads_;
    std::shared_ptr<GuestAllocator> allocator_;
    GuestAllocation stack_{};
    std::optional<psprecomp::AllegrexContext> saved_;
    std::uint32_t owner_ = 0;
    psprecomp::Runtime* runtime_ = nullptr;
    std::deque<std::uint64_t> pending_;
};
std::shared_ptr<InterruptDelivery> register_interrupt_delivery(psprecomp::Runtime&,
    std::shared_ptr<InterruptState>, std::shared_ptr<ThreadRegistry>, std::shared_ptr<GuestAllocator>);
}
