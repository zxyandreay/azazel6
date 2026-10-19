#include "azazel6/threads.hpp"
#include "psprecomp/common.hpp"
#include <algorithm>

namespace azazel6 {
namespace {
constexpr std::uint32_t no_memory = 0x80020190u;
constexpr std::uint32_t no_fill = 0x00100000u, clear_stack = 0x00200000u;
bool ram_range(const psprecomp::GuestMemory& mem, std::uint32_t begin, std::uint32_t end) {
    return begin >= psprecomp::GuestMemory::kPhysicalBase && begin < end &&
        static_cast<std::uint64_t>(end) <= psprecomp::GuestMemory::kPhysicalBase +
            static_cast<std::uint64_t>(mem.size());
}
}

std::shared_ptr<ThreadRegistry> register_thread_hle(psprecomp::Runtime& rt, ThreadArena arena,
                                                 std::shared_ptr<GuestAllocator> allocator) {
    if (!ram_range(rt.memory(), arena.begin, arena.end) || ((arena.begin | arena.end) & 255u) ||
        !ram_range(rt.memory(), arena.image_begin, arena.image_end) ||
        arena.begin < arena.image_end || arena.executable_begin < arena.image_begin ||
        arena.executable_end > arena.image_end || arena.executable_begin >= arena.executable_end ||
        arena.module_gp < arena.image_begin || arena.module_gp >= arena.image_end)
        throw psprecomp::Error("Invalid thread arena/image ownership");
    if (arena.bootstrap_priority && (arena.bootstrap_priority < 8 || arena.bootstrap_priority > 119 ||
        arena.bootstrap_stack_size < 512 || (arena.bootstrap_stack_size & 255u) ||
        static_cast<std::uint64_t>(arena.end) + arena.bootstrap_stack_size >
            psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(rt.memory().size()) ||
        arena.image_begin <= kThreadReturnGate + 4))
        throw psprecomp::Error("Invalid bootstrap ownership");
    auto state = std::shared_ptr<ThreadRegistry>(new ThreadRegistry(arena));
    if (!allocator) allocator = std::make_shared<GuestAllocator>(arena.begin, arena.end);
    if (allocator->begin() != arena.begin || allocator->end() != arena.end)
        throw psprecomp::Error("Thread allocator ownership mismatch");
    state->allocator_ = std::move(allocator);
    rt.register_hle("ThreadManForUser", 0x55C20A00u,
        [state](auto& runtime, auto& c) { state->create_event(runtime, c); });
    rt.register_hle("ThreadManForUser", 0xEF9E4C70u,
        [state](auto& runtime, auto& c) { state->delete_event(runtime, c); });
    rt.register_hle("ThreadManForUser", 0x1FB15A32u,
        [state](auto& runtime, auto& c) { state->update_event(runtime, c, false); });
    rt.register_hle("ThreadManForUser", 0x812346E4u,
        [state](auto& runtime, auto& c) { state->update_event(runtime, c, true); });
    rt.register_hle("ThreadManForUser", 0x30FD48F0u,
        [state](auto& runtime, auto& c) { state->poll_event(runtime, c, true); });
    rt.register_hle("ThreadManForUser", 0x402FCF22u,
        [state](auto& runtime, auto& c) { state->poll_event(runtime, c, false); });
    rt.register_hle("ThreadManForUser", 0xC07BB470u,
        [state](auto& runtime, auto& c) { state->create_pool(runtime, c); });
    rt.register_hle("ThreadManForUser", 0xED1410E0u,
        [state](auto& runtime, auto& c) { state->delete_pool(runtime, c); });
    rt.register_hle("ThreadManForUser", 0x623AE665u,
        [state](auto& runtime, auto& c) { state->allocate_pool(runtime, c, true); });
    rt.register_hle("ThreadManForUser", 0xD979E9BFu,
        [state](auto& runtime, auto& c) { state->allocate_pool(runtime, c, false); });
    rt.register_hle("ThreadManForUser", 0xF6414A71u,
        [state](auto& runtime, auto& c) { state->free_pool(runtime, c); });
    rt.register_hle("Kernel_Library", 0x092968F4u, [state](auto&, auto& ctx) {
        ctx.set_gpr(2, state->interrupts_enabled_ ? 1u : 0u);
        state->interrupts_enabled_ = false;
    });
    const auto resume = [state](auto& runtime, auto& ctx) {
        if (ctx.gpr[4] > 1) { runtime.stop("Unsupported interrupt controller flags"); return; }
        state->interrupts_enabled_ = ctx.gpr[4] != 0;
        // Firmware's resume service is void and leaves v0 untouched.
        // No asynchronous interrupt sources exist in this bounded runtime yet.
    };
    rt.register_hle("Kernel_Library", 0x5F10D406u, resume);
    rt.register_hle("Kernel_Library", 0x3B84732Du, resume);
    rt.register_hle("ThreadManForUser", 0xB7D098C6u,
        [state](auto& runtime, auto& ctx) { state->create_mutex(runtime, ctx); });
    rt.register_hle("ThreadManForUser", 0xF8170FBEu,
        [state](auto& runtime, auto& ctx) { state->delete_mutex(runtime, ctx); });
    rt.register_hle("ThreadManForUser", 0xB011B11Fu,
        [state](auto& runtime, auto& ctx) { state->lock_mutex(runtime, ctx, false); });
    rt.register_hle("ThreadManForUser", 0x0DDCD2C9u,
        [state](auto& runtime, auto& ctx) { state->lock_mutex(runtime, ctx, true); });
    rt.register_hle("ThreadManForUser", 0x6B30100Fu,
        [state](auto& runtime, auto& ctx) { state->unlock_mutex(runtime, ctx); });
    rt.register_hle("ThreadManForUser", 0x94AA61EEu, [state](auto& runtime, auto& ctx) {
        const auto* current = state->find(state->current_uid_);
        if (!current || current->status != ThreadStatus::Running) {
            runtime.stop("Current priority query requires a running thread"); return;
        }
        ctx.set_gpr(2, current->priority);
    });
    rt.register_hle("ThreadManForUser", 0x20FFF560u,
        [state](auto& runtime, auto& ctx) { state->create_timer(runtime, ctx); });
    rt.register_hle("ThreadManForUser", 0xE81CAF8Fu,
        [state](auto& runtime, auto& ctx) { state->create_callback(runtime, ctx); });
    rt.register_hle("ThreadManForUser", 0xEDBA5844u, [state](auto& runtime, auto& ctx) {
        if (!state->callback_context(runtime)) return;
        const auto uid = ctx.gpr[4];
        if (!state->callbacks_.erase(uid)) { ctx.set_gpr(2, 0x800201A1u); return; }
        if (state->exit_callback_ == uid) state->exit_callback_ = 0;
        ctx.set_gpr(2, 0);
    });
    rt.register_hle("LoadExecForUser", 0x4AC57943u, [state](auto& runtime, auto& ctx) {
        if (!state->callback_context(runtime)) return;
        // This exact game profile uses SDK 5.05: invalid IDs are not accepted.
        if (!state->callbacks_.contains(ctx.gpr[4])) { ctx.set_gpr(2, 0x800200D2u); return; }
        state->exit_callback_ = ctx.gpr[4];
        ctx.set_gpr(2, 0);
        // No exit event is pending in this bounded host; registration never
        // executes guest code or treats a callback as an ordinary thread.
    });
    rt.register_hle("ThreadManForUser", 0x328F9E52u, [state](auto& runtime, auto& ctx) {
        if (!state->current_uid_ || !state->interrupts_enabled_) {
            runtime.stop("Timer deletion requires a running thread with interrupts enabled"); return;
        }
        ctx.set_gpr(2, state->timers_.erase(ctx.gpr[4]) ? 0 : 0x800201BEu);
    });
    rt.register_hle("ThreadManForUser", 0xC68D9437u, [state](auto& runtime, auto& ctx) {
        if (!state->current_uid_ || !state->interrupts_enabled_) {
            runtime.stop("Timer start requires a running thread with interrupts enabled"); return;
        }
        if (!ctx.gpr[4]) { ctx.set_gpr(2, 0x800201BFu); return; }
        if (!state->timers_.contains(ctx.gpr[4])) { ctx.set_gpr(2, 0x800201BEu); return; }
        runtime.stop("Virtual timer start requires a verified guest clock and callback scheduler");
    });
    rt.nids().add("ThreadManForUser", kCreateThreadNid, "sceKernelCreateThread");
    rt.nids().add("ThreadManForUser", kDeleteThreadNid, "sceKernelDeleteThread");
    rt.register_hle("ThreadManForUser", kCreateThreadNid,
        [state](auto& runtime, auto& ctx) { state->create(runtime, ctx); });
    rt.register_hle("ThreadManForUser", kDeleteThreadNid,
        [state](auto& runtime, auto& ctx) { state->erase(runtime, ctx); });
    rt.register_hle("ThreadManForUser", kStartThreadNid,
        [state](auto& runtime, auto& ctx) { state->start(runtime, ctx); });
    rt.register_hle("ThreadManForUser", 0x71BC9871u,
        [state](auto& runtime, auto& ctx) { state->change_priority(runtime, ctx); });
    rt.register_hle("ThreadManForUser", 0xEA748E31u, [state](auto& runtime, auto& ctx) {
        auto* current = state->find(state->current_uid_);
        if (!current || current->status != ThreadStatus::Running || !state->interrupts_enabled_) {
            runtime.stop("Attribute change requires a running thread with interrupts enabled"); return;
        }
        constexpr std::uint32_t vfpu = 0x4000u;
        const auto remove = ctx.gpr[4], add = ctx.gpr[5];
        if ((remove | add) & ~vfpu) { ctx.set_gpr(2, 0x80020191u); return; }
        if (remove & add) {
            runtime.stop("Overlapping thread attribute masks require verified precedence"); return;
        }
        // The scheduler already saves every VFPU lane and control register with
        // the owning CPU context. Attribute changes must not reset that state.
        current->attributes = (current->attributes & ~remove) | add;
        ctx.set_gpr(2, 0);
    });
    rt.register_hle("ThreadManForUser", 0xAA73C935u,
        [state](auto& runtime, auto& ctx) { state->finish(runtime, ctx, false, false); });
    rt.register_hle("ThreadManForUser", 0x809CE29Bu,
        [state](auto& runtime, auto& ctx) { state->finish(runtime, ctx, true, false); });
    rt.register_hle("Azazel6Host", 1,
        [state](auto& runtime, auto& ctx) { state->finish(runtime, ctx, false, true); });
    rt.register_function(kThreadReturnGate, [](auto& runtime, auto& ctx) {
        runtime.invoke_import("Azazel6Host", 1, ctx);
    }, "host_thread_return");
    if (arena.bootstrap_priority) {
        state->bootstrap_ = {kBootstrapUid, rt.cpu().pc, arena.bootstrap_priority, 0x800000FFu,
                            arena.end, arena.bootstrap_stack_size, arena.module_gp, "module_start"};
        state->prepare(rt, state->bootstrap_, {});
        state->bootstrap_.status = ThreadStatus::Running;
        state->current_uid_ = kBootstrapUid;
        rt.cpu() = state->bootstrap_.context;
        psprecomp::set_runtime_thread_identity(kBootstrapUid, "module_start");
    }
    return state;
}

void ThreadRegistry::create(psprecomp::Runtime& rt, psprecomp::AllegrexContext& ctx) {
    if (!interrupts_enabled_) { rt.stop("Thread mutation with interrupts suspended is unsupported"); return; }
    auto& mem = rt.memory();
    // Unsupported valid PSP configurations are research stops, never fabricated guest errors.
    if (ctx.gpr[9] != 0 || (ctx.gpr[8] & ~(0x80000000u | no_fill | clear_stack)) != 0) {
        rt.stop("Unsupported thread options/attributes");
        return;
    }
    if (ctx.gpr[4] == 0) { ctx.set_gpr(2, 0x80020001u); return; }
    std::string name;
    for (std::uint32_t i = 0; i < 31; ++i) {
        const auto address = static_cast<std::uint64_t>(ctx.gpr[4]) + i;
        // Canonical RAM only until alias/privilege validation has a separate contract.
        if (address < psprecomp::GuestMemory::kPhysicalBase ||
            address >= psprecomp::GuestMemory::kPhysicalBase + static_cast<std::uint64_t>(mem.size())) {
            rt.stop("Unsupported thread name address");
            return;
        }
        const auto byte = mem.load8(static_cast<std::uint32_t>(address));
        if (!byte) break;
        name.push_back(static_cast<char>(byte));
    }
    if (ctx.gpr[6] < 8 || ctx.gpr[6] > 119) { ctx.set_gpr(2, 0x80020193u); return; }
    if (ctx.gpr[7] < 512) { ctx.set_gpr(2, 0x80020194u); return; }
    // Null entry is permitted for dormant objects. Other uncovered layouts stop.
    if (ctx.gpr[5] && ((ctx.gpr[5] & 3u) || ctx.gpr[5] < arena_.executable_begin ||
        static_cast<std::uint64_t>(ctx.gpr[5]) + 4 > arena_.executable_end)) {
        rt.stop("Unsupported thread entry address");
        return;
    }
    const auto rounded = (static_cast<std::uint64_t>(ctx.gpr[7]) + 255u) & ~std::uint64_t{255u};
    if (rounded > arena_.end - arena_.begin || next_uid_ >= 0x10000000u) {
        ctx.set_gpr(2, no_memory); return;
    }
    const auto allocation = allocator_->allocate(ctx.gpr[7], 256, true);
    if (!allocation) { ctx.set_gpr(2, no_memory); return; }
    const auto size = static_cast<std::uint32_t>(rounded);
    const auto base = allocation->base;
    // Allocate host metadata before guest writes: allocation failure cannot consume a UID/stack.
    try {
        threads_.push_back({next_uid_, ctx.gpr[5], ctx.gpr[6], ctx.gpr[8] | 0x800000FFu,
                            base, size, arena_.module_gp, std::move(name)});
    } catch (...) { allocator_->release(allocation->ticket); throw; }
    threads_.back().stack_ticket = allocation->ticket;
    if (!(ctx.gpr[8] & no_fill))
        for (std::uint32_t offset = 0; offset < size; offset += 4) mem.store32(base + offset, 0xFFFFFFFFu);
    ctx.set_gpr(2, next_uid_++);
}

void ThreadRegistry::erase(psprecomp::Runtime& rt, psprecomp::AllegrexContext& ctx) {
    if (!interrupts_enabled_) { rt.stop("Thread mutation with interrupts suspended is unsupported"); return; }
    const auto found = std::find_if(threads_.begin(), threads_.end(),
        [&](const auto& thread) { return thread.uid == ctx.gpr[4]; });
    if (found == threads_.end()) { ctx.set_gpr(2, ctx.gpr[4] ? 0x80020198u : 0x80020197u); return; }
    if (found->status != ThreadStatus::Dormant) { ctx.set_gpr(2, 0x800201A4u); return; }
    if (found->attributes & clear_stack)
        for (std::uint32_t offset = 0; offset < found->stack_size; offset += 4)
            rt.memory().store32(found->stack_base + offset, 0);
    allocator_->release(found->stack_ticket);
    std::erase_if(callbacks_, [this, uid = found->uid](const auto& item) {
        if (item.second.owner != uid) return false;
        if (exit_callback_ == item.first) exit_callback_ = 0;
        return true;
    });
    threads_.erase(found);
    ctx.set_gpr(2, 0);
}

GuestThread* ThreadRegistry::find(std::uint32_t uid) {
    if (uid == kBootstrapUid && arena_.bootstrap_priority) return &bootstrap_;
    const auto found = std::find_if(threads_.begin(), threads_.end(),
        [uid](const auto& thread) { return thread.uid == uid; });
    return found == threads_.end() ? nullptr : &*found;
}

void ThreadRegistry::prepare(psprecomp::Runtime& rt, GuestThread& thread,
                             const std::vector<std::uint8_t>& args) {
    // PSP user-thread TLS occupies the final 256 bytes. Arguments precede it,
    // aligned to 16, with 64 bytes of argument home below the initial SP.
    const auto tls = thread.stack_base + thread.stack_size - 256;
    const auto arg_address = tls - ((static_cast<std::uint32_t>(args.size()) + 15u) & ~15u);
    if (!(thread.attributes & no_fill))
        for (std::uint32_t a = thread.stack_base; a < tls; a += 4) rt.memory().store32(a, 0xFFFFFFFFu);
    for (std::uint32_t a = tls; a < tls + 256; a += 4) rt.memory().store32(a, 0);
    rt.memory().store32(tls + 192, thread.uid);
    rt.memory().store32(tls + 200, thread.stack_base);
    rt.memory().store32(tls + 248, 0xFFFFFFFFu);
    rt.memory().store32(tls + 252, 0xFFFFFFFFu);
    rt.memory().store32(thread.stack_base, thread.uid);
    for (std::size_t i = 0; i < args.size(); ++i) rt.memory().store8(arg_address + static_cast<std::uint32_t>(i), args[i]);
    thread.context = {};
    thread.context.pc = thread.entry;
    thread.context.gpr[4] = static_cast<std::uint32_t>(args.size());
    thread.context.gpr[5] = args.empty() ? 0 : arg_address;
    thread.context.gpr[26] = tls;
    thread.context.gpr[28] = thread.gp;
    thread.context.gpr[29] = arg_address - 64;
    thread.context.gpr[31] = kThreadReturnGate;
}

void ThreadRegistry::start(psprecomp::Runtime& rt, psprecomp::AllegrexContext& ctx) {
    if (!interrupts_enabled_) { rt.stop("Thread mutation with interrupts suspended is unsupported"); return; }
    if (!current_uid_) { rt.stop("Bootstrap scheduler is not configured"); return; }
    auto* thread = find(ctx.gpr[4]);
    if (!thread) { ctx.set_gpr(2, ctx.gpr[4] ? 0x80020198u : 0x80020197u); return; }
    if (thread->status != ThreadStatus::Dormant) { ctx.set_gpr(2, 0x800201A4u); return; }
    const auto size = ctx.gpr[6] ? ctx.gpr[5] : 0u;
    if (ctx.gpr[5] & 0x80000000u) { ctx.set_gpr(2, 0x800200D3u); return; }
    if (size && (ctx.gpr[6] < psprecomp::GuestMemory::kPhysicalBase ||
        static_cast<std::uint64_t>(ctx.gpr[6]) + size > psprecomp::GuestMemory::kPhysicalBase +
            static_cast<std::uint64_t>(rt.memory().size()))) { ctx.set_gpr(2, 0x800200D3u); return; }
    if (static_cast<std::uint64_t>(size) + 15 + 256 + 64 + 16 > thread->stack_size) {
        rt.stop("Thread arguments exceed owned stack"); return;
    }
    if (!thread->entry || !rt.has_function(thread->entry)) {
        rt.stop("Thread entry lacks AOT coverage " + psprecomp::hex32(thread->entry)); return;
    }
    // Snapshot before stack initialization, including when input aliases the destination.
    std::vector<std::uint8_t> args(size);
    for (std::uint32_t i = 0; i < size; ++i) args[i] = rt.memory().load8(ctx.gpr[6] + i);
    prepare(rt, *thread, args);
    thread->status = ThreadStatus::Ready;
    ctx.set_gpr(2, 0);
    ctx.pc = ctx.gpr[31];
    schedule(rt, ctx);
}

void ThreadRegistry::change_priority(psprecomp::Runtime& rt, psprecomp::AllegrexContext& ctx) {
    auto* current = find(current_uid_);
    if (!current || current->status != ThreadStatus::Running || !interrupts_enabled_) {
        rt.stop("Priority mutation requires a running thread with interrupts enabled"); return;
    }
    auto* target = ctx.gpr[4] ? find(ctx.gpr[4]) : current;
    if (!target) { ctx.set_gpr(2, 0x80020198u); return; }
    if (target->status == ThreadStatus::Dormant) { ctx.set_gpr(2, 0x800201A2u); return; }
    const auto priority = ctx.gpr[5] ? ctx.gpr[5] : current->priority;
    if (priority < 8 || priority > 119) { ctx.set_gpr(2, 0x80020193u); return; }
    const auto tied = [&](const GuestThread& other) {
        return other.uid != target->uid && other.priority == priority &&
            (other.status == ThreadStatus::Ready || other.status == ThreadStatus::Running);
    };
    if (tied(bootstrap_) || std::any_of(threads_.begin(), threads_.end(), tied)) {
        rt.stop("Equal-priority queue rotation is not implemented"); return;
    }
    target->priority = priority;
    ctx.set_gpr(2, 0);
    ctx.pc = ctx.gpr[31];
    schedule(rt, ctx);
}

void ThreadRegistry::schedule(psprecomp::Runtime& rt, psprecomp::AllegrexContext& ctx) {
    auto* current = find(current_uid_);
    GuestThread* best = current && current->status == ThreadStatus::Running ? current : nullptr;
    auto consider = [&](GuestThread& thread) {
        if (thread.status == ThreadStatus::Ready && (!best || thread.priority < best->priority)) best = &thread;
    };
    consider(bootstrap_);
    for (auto& thread : threads_) consider(thread);
    if (best == current && best) return;
    if (!best) { current_uid_ = 0; rt.stop("No runnable guest threads"); return; }
    if (current && current->status == ThreadStatus::Running) {
        current->context = ctx;
        current->status = ThreadStatus::Ready;
    }
    best->status = ThreadStatus::Running;
    current_uid_ = best->uid;
    ctx = best->context;
    psprecomp::set_runtime_thread_identity(static_cast<std::int32_t>(best->uid), best->name);
}

void ThreadRegistry::finish(psprecomp::Runtime& rt, psprecomp::AllegrexContext& ctx,
                            bool remove, bool from_return) {
    if (!interrupts_enabled_) { rt.stop("Thread exit with interrupts suspended is unsupported"); return; }
    auto* current = find(current_uid_);
    if (!current || current->status != ThreadStatus::Running) { rt.stop("No running thread to exit"); return; }
    const auto uid = current_uid_;
    if ((!remove || uid == kBootstrapUid) && std::any_of(callbacks_.begin(), callbacks_.end(), [uid](const auto& item) {
            return item.second.owner == uid;
        })) {
        rt.stop("Callback owner exit without deletion requires verified lifetime semantics"); return;
    }
    current->exit_status = ctx.gpr[from_return ? 2 : 4];
    current->context = ctx;
    current->status = ThreadStatus::Dormant;
    for (auto& [id, mutex] : mutexes_) if (mutex.owner == uid) { mutex.owner = 0; mutex.count = 0; }
    schedule(rt, ctx);
    if (remove && uid != kBootstrapUid) {
        auto temporary = ctx;
        temporary.gpr[4] = uid;
        erase(rt, temporary);
    }
}

bool ThreadRegistry::mutex_context(psprecomp::Runtime& rt) {
    if (current_uid_ && interrupts_enabled_) return true;
    rt.stop("Mutex operation requires a running thread with interrupts enabled");
    return false;
}
void ThreadRegistry::create_mutex(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!mutex_context(rt)) return;
    const auto attr = c.gpr[5], count = c.gpr[6];
    if (attr & ~0xBFFu) { c.set_gpr(2, 0x80020191u); return; }
    if ((attr & ~0x300u) || c.gpr[7]) { rt.stop("Unsupported mutex attributes/options"); return; }
    if (count > 0x7FFFFFFFu || (!(attr & 0x200u) && count > 1)) { c.set_gpr(2, 0x800201BDu); return; }
    if (!c.gpr[4]) { c.set_gpr(2, 0x80020001u); return; }
    std::string name;
    for (std::uint32_t i = 0; i < 31; ++i) {
        const auto address = static_cast<std::uint64_t>(c.gpr[4]) + i;
        if (address < 0x08000000u || address >= 0x08000000ull + rt.memory().size()) {
            rt.stop("Unsupported mutex name address"); return;
        }
        const auto byte = rt.memory().load8(static_cast<std::uint32_t>(address));
        if (!byte) break;
        name.push_back(static_cast<char>(byte));
    }
    // Bounded host metadata; UID ranges cannot overlap threads, heaps or modules.
    if (mutexes_.size() >= 4096 || next_mutex_uid_ >= 0x40000000u) { c.set_gpr(2, no_memory); return; }
    mutexes_.emplace(next_mutex_uid_, GuestMutex{std::move(name), attr, count, count, count ? current_uid_ : 0});
    c.set_gpr(2, next_mutex_uid_++);
}
void ThreadRegistry::delete_mutex(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!mutex_context(rt)) return;
    c.set_gpr(2, mutexes_.erase(c.gpr[4]) ? 0 : 0x800201C3u);
}
void ThreadRegistry::lock_mutex(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c, bool try_only) {
    if (!mutex_context(rt)) return;
    const auto found = mutexes_.find(c.gpr[4]);
    if (found == mutexes_.end()) { c.set_gpr(2, 0x800201C3u); return; }
    auto& m = found->second;
    const auto count = c.gpr[5];
    if (!count || count > 0x7FFFFFFFu || (!(m.attributes & 0x200u) && count != 1)) {
        c.set_gpr(2, 0x800201BDu); return;
    }
    if (!try_only && c.gpr[6]) { rt.stop("Mutex timeout is not implemented"); return; }
    if (m.owner && m.owner != current_uid_) {
        if (try_only) c.set_gpr(2, 0x800201C4u);
        else rt.stop("Contended mutex wait is not implemented");
        return;
    }
    if (m.owner && !(m.attributes & 0x200u)) { c.set_gpr(2, 0x800201C8u); return; }
    if (count > 0x7FFFFFFFu - m.count) { c.set_gpr(2, 0x800201C6u); return; }
    m.owner = current_uid_; m.count += count; c.set_gpr(2, 0);
}
void ThreadRegistry::unlock_mutex(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!mutex_context(rt)) return;
    const auto found = mutexes_.find(c.gpr[4]);
    if (found == mutexes_.end()) { c.set_gpr(2, 0x800201C3u); return; }
    auto& m = found->second;
    const auto count = c.gpr[5];
    if (!count || count > 0x7FFFFFFFu || (!(m.attributes & 0x200u) && count != 1)) {
        c.set_gpr(2, 0x800201BDu); return;
    }
    if (m.owner != current_uid_ || !m.count) { c.set_gpr(2, 0x800201C5u); return; }
    if (count > m.count) { c.set_gpr(2, 0x800201C7u); return; }
    m.count -= count; if (!m.count) m.owner = 0; c.set_gpr(2, 0);
}
void ThreadRegistry::create_timer(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!current_uid_ || !interrupts_enabled_) {
        rt.stop("Timer creation requires a running thread with interrupts enabled"); return;
    }
    if (c.gpr[5]) { rt.stop("Virtual timer options are unsupported"); return; }
    if (!c.gpr[4]) { c.set_gpr(2, 0x80020001u); return; }
    std::string name;
    for (std::uint32_t i = 0; i < 31; ++i) {
        const auto address = static_cast<std::uint64_t>(c.gpr[4]) + i;
        if (address < 0x08000000u || address >= 0x08000000ull + rt.memory().size()) {
            rt.stop("Unsupported virtual timer name address"); return;
        }
        const auto byte = rt.memory().load8(static_cast<std::uint32_t>(address));
        if (!byte) break;
        name.push_back(static_cast<char>(byte));
    }
    if (timers_.size() >= 4096 || next_timer_uid_ >= 0x50000000u) { c.set_gpr(2, no_memory); return; }
    timers_.emplace(next_timer_uid_, GuestVTimer{std::move(name)});
    c.set_gpr(2, next_timer_uid_++);
}
bool ThreadRegistry::callback_context(psprecomp::Runtime& rt) {
    const auto* current = find(current_uid_);
    if (current && current->status == ThreadStatus::Running && interrupts_enabled_) return true;
    rt.stop("Callback operation requires a running thread with interrupts enabled"); return false;
}
void ThreadRegistry::create_callback(psprecomp::Runtime& rt, psprecomp::AllegrexContext& c) {
    if (!callback_context(rt)) return;
    if (!c.gpr[4]) { c.set_gpr(2, 0x80020001u); return; }
    const auto function = c.gpr[5];
    if (function && ((function & 3u) || function < arena_.executable_begin ||
                    static_cast<std::uint64_t>(function) + 4 > arena_.executable_end)) {
        rt.stop("Callback function outside verified executable layout"); return;
    }
    std::string name;
    for (std::uint32_t i = 0; i < 31; ++i) {
        const auto address = static_cast<std::uint64_t>(c.gpr[4]) + i;
        if (address < 0x08000000u || address >= 0x08000000ull + rt.memory().size()) {
            rt.stop("Unsupported callback name address"); return;
        }
        const auto byte = rt.memory().load8(static_cast<std::uint32_t>(address));
        if (!byte) break;
        name.push_back(static_cast<char>(byte));
    }
    if (callbacks_.size() >= 4096 || next_callback_uid_ >= 0x60000000u) { c.set_gpr(2, no_memory); return; }
    callbacks_.emplace(next_callback_uid_, GuestCallback{std::move(name), current_uid_, function, c.gpr[6]});
    c.set_gpr(2, next_callback_uid_++);
}
} // namespace azazel6
