#pragma once
#include "kernel/kernel.hpp"

namespace azazel6 {
// The caller checks the active thread and interrupt context before this change.
inline std::uint32_t cancel_wakeup(portablekit::Thread* target) {
    if (!target) return portablekit::error::kUnknownThid;
    if (target->status == portablekit::ThreadStatus::Dead)
        return portablekit::error::kUnknownThid;
    if (target->status == portablekit::ThreadStatus::Dormant)
        return portablekit::error::kDormant;
    const auto pending = target->wakeup_count;
    target->wakeup_count = 0;
    return pending;
}
} // namespace azazel6
