#include "azazel6/threads.hpp"

namespace azazel6 {
bool ThreadRegistry::event_output(psprecomp::Runtime& rt, std::uint32_t address) const {
    return !address || (!(address & 3u) && address >= arena_.executable_end &&
        static_cast<std::uint64_t>(address) + 4 <= psprecomp::GuestMemory::kPhysicalBase +
            static_cast<std::uint64_t>(rt.memory().size()) && !rt.has_function(address));
}
bool ThreadRegistry::wake_event(psprecomp::Runtime& rt, std::uint32_t uid, GuestEventFlag& event,
                                std::uint32_t pattern, std::uint32_t result) {
    // Validate every retained waiter before any pattern/output/context mutation.
    for (const auto& wait : event.waiters) {
        const auto* thread = find(wait.thread);
        if (!thread || thread->status != ThreadStatus::Waiting || thread->event_wait != uid ||
            !event_output(rt, wait.output)) {
            rt.stop("Event waiter ownership or result address changed"); return false;
        }
    }
    event.pattern = pattern;
    for (auto it = event.waiters.begin(); it != event.waiters.end();) {
        const bool matched = (it->mode & 1u) ? (event.pattern & it->bits) != 0 : (event.pattern & it->bits) == it->bits;
        if (!result && !matched) { ++it; continue; }
        if (it->output) rt.memory().store32(it->output, event.pattern);
        if (!result) {
            if (it->mode & 0x10u) event.pattern = 0;
            else if (it->mode & 0x20u) event.pattern &= ~it->bits;
        }
        auto* thread = find(it->thread);
        thread->event_wait.reset(); thread->context.set_gpr(2, result); thread->status = ThreadStatus::Ready;
        it = event.waiters.erase(it);
    }
    return true;
}
bool ThreadRegistry::event_context(psprecomp::Runtime& rt) {
    const auto* current = find(current_uid_);
    if (current && current->status == ThreadStatus::Running && interrupts_enabled_) return true;
    rt.stop("Event flag operation requires a running thread with interrupts enabled"); return false;
}
void ThreadRegistry::create_event(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!event_context(rt)) return;
    if (!c.gpr[4]) { c.set_gpr(2, 0x80020001u); return; }
    const auto attr = c.gpr[5];
    if (attr & ~0x2FFu) { c.set_gpr(2, 0x80020191u); return; }
    if (attr & ~0x200u) { rt.stop("Unsupported event flag attribute semantics"); return; }
    if (c.gpr[7]) { rt.stop("Event flag creation options are unimplemented"); return; }
    std::string name;
    for (std::uint32_t i = 0; i < 31; ++i) {
        const auto address = static_cast<std::uint64_t>(c.gpr[4]) + i;
        if (address < psprecomp::GuestMemory::kPhysicalBase || address >=
            psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(rt.memory().size())) {
            rt.stop("Unsupported event flag name address"); return;
        }
        const auto byte = rt.memory().load8(static_cast<std::uint32_t>(address));
        if (!byte) break;
        name.push_back(static_cast<char>(byte));
    }
    if (event_flags_.size() >= 4096 || next_event_uid_ >= 0x71000000u) { c.set_gpr(2, 0x80020190u); return; }
    event_flags_.emplace(next_event_uid_, GuestEventFlag{std::move(name), attr, c.gpr[6], c.gpr[6]});
    c.set_gpr(2, next_event_uid_++);
}
void ThreadRegistry::delete_event(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!event_context(rt)) return;
    const auto found = event_flags_.find(c.gpr[4]);
    if (found == event_flags_.end()) { c.set_gpr(2, 0x8002019Au); return; }
    if (!wake_event(rt, found->first, found->second, found->second.pattern, 0x800201B5u)) return;
    event_flags_.erase(found);
    c.set_gpr(2, 0); c.pc = c.gpr[31]; reschedule(rt, c);
}
void ThreadRegistry::cancel_event(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!event_context(rt)) return;
    const auto found = event_flags_.find(c.gpr[4]);
    if (found == event_flags_.end()) { c.set_gpr(2, 0x8002019Au); return; }
    if (!event_output(rt, c.gpr[6])) { rt.stop("Unsupported event cancellation result address"); return; }
    const auto count = static_cast<std::uint32_t>(found->second.waiters.size());
    // Reject aliased outputs until their firmware write ordering is established.
    for (const auto& wait : found->second.waiters)
        if (c.gpr[6] && c.gpr[6] == wait.output) { rt.stop("Aliased event cancellation outputs"); return; }
    if (!wake_event(rt, found->first, found->second, c.gpr[5], 0x800201A9u)) return;
    if (c.gpr[6]) rt.memory().store32(c.gpr[6], count);
    c.set_gpr(2, 0); c.pc = c.gpr[31]; reschedule(rt, c);
}
void ThreadRegistry::update_event(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c, bool clear) {
    if (!event_context(rt)) return;
    const auto found = event_flags_.find(c.gpr[4]);
    if (found == event_flags_.end()) { c.set_gpr(2, 0x8002019Au); return; }
    auto& pattern = found->second.pattern;
    // ClearEventFlag takes the mask to retain, not the bits to remove.
    if (clear) pattern &= c.gpr[5];
    else if (!wake_event(rt, found->first, found->second, pattern | c.gpr[5], 0)) return;
    c.set_gpr(2, 0);
    if (!clear) { c.pc = c.gpr[31]; reschedule(rt, c); }
}
void ThreadRegistry::poll_event(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c, bool try_only) {
    if (!event_context(rt)) return;
    const auto bits = c.gpr[5], mode = c.gpr[6], output = c.gpr[7];
    if ((mode & ~0x31u) || (try_only && (mode & 0x30u) == 0x30u)) { c.set_gpr(2, 0x80020195u); return; }
    if (!bits) { c.set_gpr(2, 0x800201B1u); return; }
    const auto found = event_flags_.find(c.gpr[4]);
    if (found == event_flags_.end()) { c.set_gpr(2, 0x8002019Au); return; }
    if (!try_only && c.gpr[8]) { rt.stop("Event flag timeouts are unimplemented"); return; }
    if (!event_output(rt, output)) {
        rt.stop("Unsupported event flag result address"); return;
    }
    auto& pattern = found->second.pattern;
    const auto matched = (mode & 1u) ? (pattern & bits) != 0 : (pattern & bits) == bits;
    if (!matched && !try_only) {
        auto& waiters = found->second.waiters;
        if (!waiters.empty() && !(found->second.attributes & 0x200u)) { c.set_gpr(2, 0x800201B0u); return; }
        if (waiters.size() >= 4096) { rt.stop("Event waiter budget exhausted"); return; }
        waiters.push_back({current_uid_, bits, mode, output});
        auto* thread = find(current_uid_);
        thread->context = c; thread->context.pc = c.gpr[31];
        thread->event_wait = found->first; thread->status = ThreadStatus::Waiting;
        schedule(rt, c); return;
    }
    // A valid failed poll still reports the entire current pattern.
    if (output) rt.memory().store32(output, pattern);
    if (!matched) {
        c.set_gpr(2, !found->second.waiters.empty() && !(found->second.attributes & 0x200u)
            ? 0x800201B0u : 0x800201AFu);
        return;
    }
    if (mode & 0x10u) pattern = 0;
    else if (mode & 0x20u) pattern &= ~bits;
    c.set_gpr(2, 0);
}
} // namespace azazel6
