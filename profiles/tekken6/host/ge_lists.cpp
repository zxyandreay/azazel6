#include "azazel6/ge_lists.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>
#include <limits>

namespace azazel6 {
GeQueue::GeQueue(psprecomp::GuestMemory& memory, std::shared_ptr<GeCallbacks> callbacks)
    : memory_(memory), callbacks_(std::move(callbacks)) {
    if (!callbacks_) throw psprecomp::Error("GE queue requires callback ownership");
}
bool GeQueue::callback_live(const GeList& item) const {
    return !item.callback || (callbacks_->slots[*item.callback] &&
        callbacks_->generations[*item.callback] == item.callback_generation);
}
void GeQueue::check_word(const GeList& item, std::uint32_t address) const {
    if ((address & 3u) || address < item.begin || static_cast<std::uint64_t>(address) + 4 > item.end)
        throw psprecomp::Error("GE command target outside owned list span");
}
std::uint32_t GeQueue::enqueue(std::uint32_t begin, std::uint32_t end, std::uint32_t stall,
                               std::optional<std::uint32_t> callback) {
    if ((begin | end) & 3u || begin >= end || begin < psprecomp::GuestMemory::kPhysicalBase ||
        static_cast<std::uint64_t>(end) > psprecomp::GuestMemory::kPhysicalBase +
            static_cast<std::uint64_t>(memory_.size()))
        throw psprecomp::Error("Invalid canonical GE list span");
    if (stall && ((stall & 3u) || stall < begin || stall > end))
        throw psprecomp::Error("Invalid GE stall address");
    if (callback && (*callback >= callbacks_->slots.size() || !callbacks_->slots[*callback]))
        throw psprecomp::Error("GE callback registration not owned");
    if (lists_.size() >= 64 || next_id_ == 0x7FFFFFFFu)
        throw psprecomp::Error("GE list ownership budget exhausted");
    // Concurrent overlapping producers require a separate ownership contract.
    for (const auto& [id, item] : lists_)
        if (item.status != GeStatus::Complete && item.status != GeStatus::Failed && begin < item.end && item.begin < end)
            throw psprecomp::Error("Overlapping live GE list spans");
    GeList item{begin, end, begin, stall, callback};
    if (callback) item.callback_generation = callbacks_->generations[*callback];
    const auto id = next_id_;
    lists_.emplace(id, std::move(item));
    try { queue_.push_back(id); } catch (...) { lists_.erase(id); throw; }
    ++next_id_;
    return id;
}
const GeList& GeQueue::list(std::uint32_t id) const { return lists_.at(id); }
void GeQueue::update_stall(std::uint32_t id, std::uint32_t stall) {
    auto& item = lists_.at(id);
    if (item.status == GeStatus::Complete || item.status == GeStatus::Failed)
        throw psprecomp::Error("Cannot update a terminal GE list");
    if (stall && ((stall & 3u) || stall < item.begin || stall > item.end))
        throw psprecomp::Error("Invalid GE stall address");
    item.stall = stall;
    if (item.status == GeStatus::Stalled) item.status = GeStatus::Queued;
}
void GeQueue::erase(std::uint32_t id) {
    auto& item = lists_.at(id);
    if (item.event_taken) throw psprecomp::Error("GE callback still in flight");
    std::erase(queue_, id); lists_.erase(id);
}
void GeQueue::fail(GeList& item, std::string why) {
    item.status = GeStatus::Failed; item.error = std::move(why); item.event.reset(); item.event_taken = false;
}
void GeQueue::terminal(GeList& item, std::uint32_t id) {
    const auto command = *item.terminal;
    const bool finish = command >> 24 == 0x0Fu;
    if (!callback_live(item)) { fail(item, "GE callback registration changed"); return; }
    const auto callback = item.callback ? callbacks_->slots[*item.callback] : std::optional<GeCallback>{};
    const auto function = callback ? (finish ? callback->finish_function : callback->signal_function) : 0u;
    item.terminal.reset();
    if (!function) { item.status = finish ? GeStatus::Complete : GeStatus::Queued; return; }
    if (next_event_ == std::numeric_limits<std::uint64_t>::max()) { fail(item, "GE event generation exhausted"); return; }
    item.event = GeCallbackEvent{next_event_++, id, function,
        finish ? callback->finish_argument : callback->signal_argument, command & 0xFFFFu, finish};
    item.status = GeStatus::Callback;
}
void GeQueue::run(std::uint32_t budget) {
    if (!budget || budget > 65536) throw psprecomp::Error("Invalid GE command budget");
    if (queue_.empty()) return;
    const auto id = queue_.front(); auto& item = lists_.at(id);
    if (item.status == GeStatus::Failed || item.status == GeStatus::Callback) return;
    for (std::uint32_t used = 0; used < budget; ++used) {
        if (item.stall && item.pc == item.stall) { item.status = GeStatus::Stalled; return; }
        if (item.commands >= 65536) { fail(item, "GE list command budget exhausted"); return; }
        try {
            check_word(item, item.pc);
            const auto command = memory_.load32(item.pc), opcode = command >> 24, value = command & 0xFFFFFFu;
            if (item.terminal && opcode != 0x0Cu) { fail(item, "GE terminal command requires END"); return; }
            std::uint32_t next = item.pc + 4;
            switch (opcode) {
            case 0x00: break;
            case 0x10:
                if (value & ~0x000F0000u) { fail(item, "Unsupported GE BASE bits"); return; }
                item.base = value << 8; break;
            case 0x08: case 0x0A: {
                const auto target = item.base | value;
                check_word(item, target);
                if (opcode == 0x0A) {
                    if (item.returns.size() >= 8) { fail(item, "GE call stack budget exhausted"); return; }
                    check_word(item, next); item.returns.push_back(next);
                }
                next = target; break;
            }
            case 0x0B:
                if (value || item.returns.empty()) { fail(item, "Invalid GE return"); return; }
                next = item.returns.back(); item.returns.pop_back(); break;
            case 0x0F:
                if (value > 0xFFFFu) { fail(item, "Unsupported GE FINISH payload"); return; }
                item.terminal = command; break;
            case 0x0E:
                // Only handler-suspend SIGNAL is implemented. NOWAIT/PAUSE need distinct ordering.
                if (value >> 16 != 1) { fail(item, "Unsupported GE SIGNAL behavior"); return; }
                item.terminal = command; break;
            case 0x0C:
                if (value || !item.terminal || !item.returns.empty()) { fail(item, "Invalid GE END flow"); return; }
                terminal(item, id); break;
            default: fail(item, "Unsupported GE command " + psprecomp::hex32(command)); return;
            }
            item.pc = next; ++item.commands;
            if (item.status == GeStatus::Complete) { queue_.pop_front(); return; }
            if (item.status == GeStatus::Callback || item.status == GeStatus::Failed) return;
            item.status = GeStatus::Queued;
        } catch (const psprecomp::Error& error) { fail(item, error.what()); return; }
    }
    // Yield, retaining the independent total-list budget across subsequent calls.
}
std::optional<GeCallbackEvent> GeQueue::take_callback() {
    if (queue_.empty()) return {};
    auto& item = lists_.at(queue_.front());
    if (item.status != GeStatus::Callback || item.event_taken) return {};
    if (!callback_live(item)) { fail(item, "GE callback registration changed"); return {}; }
    item.event_taken = true; return item.event;
}
void GeQueue::complete_callback(std::uint64_t token) {
    if (queue_.empty()) throw psprecomp::Error("No pending GE callback");
    auto& item = lists_.at(queue_.front());
    if (!item.event || !item.event_taken || item.event->token != token)
        throw psprecomp::Error("GE callback completion ownership mismatch");
    if (!callback_live(item)) { fail(item, "GE callback registration changed during delivery"); return; }
    const auto finish = item.event->finish;
    item.event.reset(); item.event_taken = false;
    item.status = finish ? GeStatus::Complete : GeStatus::Queued;
    if (finish) queue_.pop_front();
}
bool dispatch_ge_callback(psprecomp::Runtime& rt, const std::shared_ptr<GeQueue>& queue,
                          InterruptDelivery& delivery, std::uint32_t gp) {
    if (!queue || !queue->owns_memory(rt.memory())) throw psprecomp::Error("GE callback queue is not owned by runtime");
    if (rt.stopped() || !delivery.available()) return false;
    const auto event = queue->take_callback();
    if (!event) return false;
    return delivery.dispatch_callback(rt, event->function, event->value, event->argument, gp,
        [queue, token = event->token] { queue->complete_callback(token); });
}
}
