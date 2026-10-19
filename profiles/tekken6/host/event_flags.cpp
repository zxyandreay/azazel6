#include "azazel6/threads.hpp"

namespace azazel6 {
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
    c.set_gpr(2, event_flags_.erase(c.gpr[4]) ? 0u : 0x8002019Au);
}
void ThreadRegistry::update_event(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c, bool clear) {
    if (!event_context(rt)) return;
    const auto found = event_flags_.find(c.gpr[4]);
    if (found == event_flags_.end()) { c.set_gpr(2, 0x8002019Au); return; }
    auto& pattern = found->second.pattern;
    // ClearEventFlag takes the mask to retain, not the bits to remove.
    if (clear) pattern &= c.gpr[5]; else pattern |= c.gpr[5];
    c.set_gpr(2, 0);
}
void ThreadRegistry::poll_event(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c, bool try_only) {
    if (!event_context(rt)) return;
    const auto bits = c.gpr[5], mode = c.gpr[6], output = c.gpr[7];
    if ((mode & ~0x31u) || (mode & 0x30u) == 0x30u) { c.set_gpr(2, 0x80020195u); return; }
    if (!bits) { c.set_gpr(2, 0x800201B1u); return; }
    const auto found = event_flags_.find(c.gpr[4]);
    if (found == event_flags_.end()) { c.set_gpr(2, 0x8002019Au); return; }
    if (!try_only && c.gpr[8]) { rt.stop("Event flag timeouts are unimplemented"); return; }
    if (output && ((output & 3u) || output < arena_.executable_end ||
        static_cast<std::uint64_t>(output) + 4 > psprecomp::GuestMemory::kPhysicalBase +
            static_cast<std::uint64_t>(rt.memory().size()) || rt.has_function(output))) {
        rt.stop("Unsupported event flag result address"); return;
    }
    auto& pattern = found->second.pattern;
    const auto matched = (mode & 1u) ? (pattern & bits) != 0 : (pattern & bits) == bits;
    if (!matched && !try_only) {
        rt.stop("Event flag wait requires a verified waiting scheduler"); return;
    }
    // A valid failed poll still reports the entire current pattern.
    if (output) rt.memory().store32(output, pattern);
    if (!matched) { c.set_gpr(2, 0x800201AFu); return; }
    if (mode & 0x10u) pattern = 0;
    else if (mode & 0x20u) pattern &= ~bits;
    c.set_gpr(2, 0);
}
} // namespace azazel6
