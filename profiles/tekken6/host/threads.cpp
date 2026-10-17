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

std::shared_ptr<ThreadRegistry> register_thread_hle(psprecomp::Runtime& rt, ThreadArena arena) {
    if (!ram_range(rt.memory(), arena.begin, arena.end) || ((arena.begin | arena.end) & 255u) ||
        !ram_range(rt.memory(), arena.image_begin, arena.image_end) ||
        arena.begin < arena.image_end || arena.executable_begin < arena.image_begin ||
        arena.executable_end > arena.image_end || arena.executable_begin >= arena.executable_end ||
        arena.module_gp < arena.image_begin || arena.module_gp >= arena.image_end)
        throw psprecomp::Error("Invalid thread arena/image ownership");
    auto state = std::shared_ptr<ThreadRegistry>(new ThreadRegistry(arena));
    rt.nids().add("ThreadManForUser", kCreateThreadNid, "sceKernelCreateThread");
    rt.nids().add("ThreadManForUser", kDeleteThreadNid, "sceKernelDeleteThread");
    rt.register_hle("ThreadManForUser", kCreateThreadNid,
        [state](auto& runtime, auto& ctx) { state->create(runtime, ctx); });
    rt.register_hle("ThreadManForUser", kDeleteThreadNid,
        [state](auto& runtime, auto& ctx) { state->erase(runtime, ctx); });
    return state;
}

void ThreadRegistry::create(psprecomp::Runtime& rt, psprecomp::AllegrexContext& ctx) {
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
    if (rounded > arena_.end - arena_.begin || next_uid_ > 0x7FFFFFFFu) {
        ctx.set_gpr(2, no_memory); return;
    }
    // Highest free fitting range. Derive holes from live ownership so delete reclaims space.
    auto ordered = threads_;
    std::sort(ordered.begin(), ordered.end(), [](const auto& a, const auto& b) {
        return a.stack_base > b.stack_base;
    });
    auto top = arena_.end;
    for (const auto& thread : ordered) {
        if (top - (thread.stack_base + thread.stack_size) >= rounded) break;
        top = thread.stack_base;
    }
    if (top - arena_.begin < rounded) { ctx.set_gpr(2, no_memory); return; }
    const auto size = static_cast<std::uint32_t>(rounded);
    const auto base = top - size;
    // Allocate host metadata before guest writes: allocation failure cannot consume a UID/stack.
    threads_.push_back({next_uid_, ctx.gpr[5], ctx.gpr[6], ctx.gpr[8] | 0x800000FFu,
                        base, size, arena_.module_gp, std::move(name)});
    if (!(ctx.gpr[8] & no_fill))
        for (std::uint32_t offset = 0; offset < size; offset += 4) mem.store32(base + offset, 0xFFFFFFFFu);
    ctx.set_gpr(2, next_uid_++);
}

void ThreadRegistry::erase(psprecomp::Runtime& rt, psprecomp::AllegrexContext& ctx) {
    const auto found = std::find_if(threads_.begin(), threads_.end(),
        [&](const auto& thread) { return thread.uid == ctx.gpr[4]; });
    if (found == threads_.end()) { ctx.set_gpr(2, ctx.gpr[4] ? 0x80020198u : 0x80020197u); return; }
    if (found->attributes & clear_stack)
        for (std::uint32_t offset = 0; offset < found->stack_size; offset += 4)
            rt.memory().store32(found->stack_base + offset, 0);
    threads_.erase(found);
    ctx.set_gpr(2, 0);
}
} // namespace azazel6
