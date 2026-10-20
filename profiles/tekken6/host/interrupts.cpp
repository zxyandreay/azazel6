#include "azazel6/interrupts.hpp"
#include "psprecomp/common.hpp"
#include <limits>
#include "azazel6/bootstrap.hpp"

namespace azazel6 {
std::shared_ptr<InterruptState> register_interrupt_hle(psprecomp::Runtime& rt,
    std::uint32_t begin, std::uint32_t end, std::uint32_t gp) {
    const auto ram_end = psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(rt.memory().size());
    if (begin < psprecomp::GuestMemory::kPhysicalBase || begin >= end || ((begin | end) & 3u) ||
        end > ram_end || gp < psprecomp::GuestMemory::kPhysicalBase || gp >= ram_end)
        throw psprecomp::Error("Invalid interrupt module ownership");
    auto state = std::make_shared<InterruptState>();
    const auto valid_slot = [](auto& runtime, auto& c) {
        if (c.gpr[4] >= 68 || c.gpr[5] >= 32) { c.set_gpr(2, 0x80020065u); return false; }
        if (c.gpr[4] != 30 || c.gpr[5] != 0) {
            runtime.stop("Unsupported subinterrupt: only VBLANK slot zero is verified"); return false;
        }
        return true;
    };
    rt.register_hle("InterruptManager", 0xCA04A2B9u, [=](auto& runtime, auto& c) {
        if (!valid_slot(runtime, c)) return;
        if (state->vblank) { c.set_gpr(2, 0x80020067u); return; }
        const auto function = c.gpr[6];
        if ((function & 3u) || function < begin || static_cast<std::uint64_t>(function) + 4 > end ||
            !runtime.has_function(function)) {
            runtime.stop("VBLANK handler requires verified executable AOT coverage"); return;
        }
        if (state->generation == std::numeric_limits<std::uint64_t>::max()) {
            runtime.stop("Interrupt generation exhausted"); return;
        }
        state->vblank = VblankHandler{function, c.gpr[7], gp};
        // Preserve an enable issued before registration, as the controller does.
        ++state->generation;
        c.set_gpr(2, 0);
    });
    rt.register_hle("InterruptManager", 0xD61E6961u, [=](auto& runtime, auto& c) {
        if (!valid_slot(runtime, c)) return;
        if (!state->vblank) { c.set_gpr(2, 0x80020068u); return; }
        state->vblank.reset(); state->enabled = false;
        c.set_gpr(2, 0);
    });
    for (const auto nid : {0xFB8E22ECu, 0x8A389411u}) {
        rt.register_hle("InterruptManager", nid, [=](auto& runtime, auto& c) {
            if (!valid_slot(runtime, c)) return;
            // The display controller owns the enable bit even before registration.
            state->enabled = nid == 0xFB8E22ECu;
            c.set_gpr(2, 0);
        });
    }
    return state;
}

InterruptDelivery::~InterruptDelivery() {
    if (allocator_ && stack_.ticket) allocator_->release(stack_.ticket);
}
std::shared_ptr<InterruptDelivery> register_interrupt_delivery(psprecomp::Runtime& rt,
    std::shared_ptr<InterruptState> state, std::shared_ptr<ThreadRegistry> threads,
    std::shared_ptr<GuestAllocator> allocator) {
    if (!state || !threads || !allocator || rt.has_function(kInterruptReturnGate) ||
        allocator->begin() <= kInterruptReturnGate + 4 ||
        static_cast<std::uint64_t>(allocator->end()) >
            psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(rt.memory().size()))
        throw psprecomp::Error("Invalid interrupt delivery ownership");
    auto delivery = std::make_shared<InterruptDelivery>();
    delivery->runtime_ = &rt;
    delivery->allocator_ = std::move(allocator);
    const auto stack = delivery->allocator_->allocate(4096, 256, true);
    if (!stack) throw psprecomp::Error("No owned interrupt stack available");
    delivery->stack_ = *stack;
    delivery->state_ = std::move(state); delivery->threads_ = std::move(threads);
    rt.register_hle("Azazel6Host", 2, [delivery](auto& runtime, auto& c) { delivery->finish(runtime, c); });
    rt.register_function(kInterruptReturnGate, [](auto& runtime, auto& c) {
        runtime.invoke_import("Azazel6Host", 2, c);
    }, "host_interrupt_return");
    return delivery;
}
void InterruptDelivery::raise_vblank() {
    // Pinned display reference drops new VBLANK requests while CPU interrupts are masked.
    if (!state_->vblank || !state_->enabled || !threads_->interrupts_enabled()) return;
    if (pending_.size() >= 1024) throw psprecomp::Error("Interrupt event budget exhausted");
    pending_.push_back(state_->generation);
}
bool InterruptDelivery::dispatch(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (&rt != runtime_ || &c != &rt.cpu()) throw psprecomp::Error("Interrupt runtime/context mismatch");
    if (rt.stopped() || saved_ || !threads_->interrupts_enabled()) return false;
    while (!pending_.empty() && (!state_->vblank || pending_.front() != state_->generation))
        pending_.pop_front();
    if (pending_.empty()) return false;
    const auto handler = *state_->vblank;
    if (!enter(rt, c, handler, 0)) return false;
    pending_.pop_front();
    return true;
}
bool InterruptDelivery::enter(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c,
                               VblankHandler handler, std::uint32_t value) {
    if (!rt.has_function(handler.function)) { rt.stop("VBLANK AOT coverage was removed"); return false; }
    if (!threads_->enter_interrupt()) { rt.stop("VBLANK requires a running thread context"); return false; }
    saved_ = c; owner_ = threads_->current_uid();
    for (auto address = stack_.base; address < stack_.base + stack_.size; address += 4)
        rt.memory().store32(address, 0);
    c.pc = handler.function; c.set_gpr(4, value); c.set_gpr(5, handler.argument);
    c.set_gpr(28, handler.gp); c.set_gpr(29, stack_.base + stack_.size - 16);
    c.set_gpr(31, kInterruptReturnGate);
    psprecomp::set_runtime_thread_identity(-2, "vblank_interrupt");
    return true;
}
bool InterruptDelivery::dispatch_callback(psprecomp::Runtime& rt, std::uint32_t function,
    std::uint32_t value, std::uint32_t argument, std::uint32_t gp, std::function<void()> completion) {
    if (&rt != runtime_) throw psprecomp::Error("Callback runtime ownership mismatch");
    if (rt.stopped() || !available()) return false;
    if (!completion || gp < psprecomp::GuestMemory::kPhysicalBase ||
        static_cast<std::uint64_t>(gp) >= psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(rt.memory().size()))
        throw psprecomp::Error("Invalid callback module/completion ownership");
    if (!enter(rt, rt.cpu(), {function, argument, gp}, value)) return false;
    completion_ = std::move(completion);
    return true;
}
void InterruptDelivery::finish(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!saved_ || !threads_->in_interrupt() || threads_->current_uid() != owner_ ||
        c.pc != kInterruptReturnGate || psprecomp::runtime_thread_uid() != -2) {
        rt.stop("Invalid interrupt return ownership"); return;
    }
    c = *saved_; saved_.reset(); owner_ = 0;
    threads_->leave_interrupt();
    auto completion = std::move(completion_);
    completion_ = {};
    if (completion) completion();
    threads_->reschedule(rt, c);
}
}
