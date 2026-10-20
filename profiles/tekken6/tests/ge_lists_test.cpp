#include "azazel6/ge_lists.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace azazel6;
constexpr std::uint32_t base = 0x08004000u, code = 0x08001000u, record = 0x08003000u;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
template<class F> void rejects(F f, const char* why) {
    bool caught = false; try { f(); } catch (const std::exception&) { caught = true; }
    check(caught, why);
}
struct Fixture {
    std::unique_ptr<psprecomp::Runtime> rt = std::make_unique<psprecomp::Runtime>();
    std::shared_ptr<GeCallbacks> callbacks = register_ge_callback_hle(*rt, code, code + 256);
    GeQueue queue{rt->memory(), callbacks};
    void words(std::initializer_list<std::uint32_t> words) {
        auto address = base;
        for (auto word : words) { rt->memory().store32(address, word); address += 4; }
    }
    void callback(bool remove = false) {
        auto& m = rt->memory();
        m.store32(record, code); m.store32(record + 4, 0x12345678u);
        m.store32(record + 8, code + 4); m.store32(record + 12, 0xFFFFFFFFu);
        rt->cpu().gpr[4] = remove ? 0 : record;
        rt->invoke_import("sceGe_user", remove ? 0x05DB22CEu : 0xA4FC06A4u, rt->cpu());
        check(rt->cpu().gpr[2] == 0, "Callback slot allocation");
    }
};
void callback_execution() {
    auto rt = std::make_unique<psprecomp::Runtime>();
    auto callbacks = register_ge_callback_hle(*rt, code, code + 256);
    auto allocator = std::make_shared<GuestAllocator>(0x08010000u, 0x08020000u);
    auto threads = register_thread_hle(*rt, {0x08010000u, 0x08020000u, code, 0x08010000u,
        code, code + 256, record, 32, 4096}, allocator);
    auto interrupt_state = register_interrupt_hle(*rt, code, code + 256, record);
    auto delivery = register_interrupt_delivery(*rt, interrupt_state, threads, allocator);
    auto queue = std::make_shared<GeQueue>(rt->memory(), callbacks);
    rt->register_function(code, [](auto& runtime, auto& c) {
        runtime.memory().store32(c.gpr[5], c.gpr[4]); c.gpr[16] = 0; c.pc = c.gpr[31];
    }, "synthetic_ge_finish");
    rt->register_function(code + 4, [](auto& runtime, auto&) { runtime.stop("synthetic GE complete"); }, "continuation");
    auto& memory = rt->memory();
    memory.store32(record, 0); memory.store32(record + 4, 0);
    memory.store32(record + 8, code); memory.store32(record + 12, record + 16);
    rt->cpu().gpr[4] = record; rt->invoke_import("sceGe_user", 0xA4FC06A4u, rt->cpu());
    memory.store32(base, 0x0F00002Au); memory.store32(base + 4, 0x0C000000u);
    const auto id = queue->enqueue(base, base + 8, 0, 0); queue->run(8);
    rt->invoke_import("Kernel_Library", 0x092968F4u, rt->cpu());
    check(!dispatch_ge_callback(*rt, queue, *delivery, record), "GE callback bypassed CPU interrupt mask");
    rt->cpu().gpr[4] = 1; rt->invoke_import("Kernel_Library", 0x5F10D406u, rt->cpu());
    rt->cpu().pc = code + 4; rt->cpu().gpr[16] = 77;
    check(dispatch_ge_callback(*rt, queue, *delivery, record) && queue->list(id).status == GeStatus::Callback,
          "GE delivery failed or completed before return");
    rt->run(rt->cpu().pc, 8);
    check(rt->stop_reason() == "synthetic GE complete" && memory.load32(record + 16) == 42 &&
          rt->cpu().gpr[16] == 77 && queue->list(id).status == GeStatus::Complete, "Guest FINISH ABI/return/acknowledgment");
}
}
int main() {
    try {
        Fixture f;
        f.words({0, 0x0F000007u, 0x0C000000u});
        const auto id = f.queue.enqueue(base, base + 12, base + 4);
        f.queue.run(16);
        check(f.queue.list(id).status == GeStatus::Stalled && f.queue.list(id).commands == 1, "Stall executed command");
        f.queue.run(16); check(f.queue.list(id).commands == 1, "Repeated stall consumed commands");
        rejects([&] { f.queue.update_stall(id, base + 1); }, "Unaligned stall");
        check(f.queue.list(id).stall == base + 4, "Bad stall changed state");
        f.queue.update_stall(id, 0); f.queue.run(1);
        check(f.queue.list(id).status == GeStatus::Queued, "FINISH completed before END");
        f.queue.run(1); check(f.queue.list(id).status == GeStatus::Complete, "FINISH/END completion");
        f.queue.erase(id);
        rejects([&] { f.queue.update_stall(id, 0); }, "Deleted list reused");
        const auto next = f.queue.enqueue(base, base + 12);
        check(next != id, "Stale list handle alias"); f.queue.erase(next);
        for (auto bounds : {std::pair{0u, base}, std::pair{base + 1, base + 12},
                            std::pair{base, base}, std::pair{0x09FFFFFCu, 0x0A000004u},
                            std::pair{0x88004000u, 0x88004010u}})
            rejects([&] { f.queue.enqueue(bounds.first, bounds.second); }, "Invalid list span");
        f.callback(); f.words({0x0E01000Bu, 0x0C000000u, 0x0F00000Cu, 0x0C000000u});
        auto cb = f.queue.enqueue(base, base + 16, 0, 0);
        f.queue.run(16); auto signal = f.queue.take_callback();
        check(signal && !signal->finish && signal->value == 11 && signal->function == code &&
              signal->argument == 0x12345678u && !f.queue.take_callback(), "SIGNAL callback identity/single delivery");
        rejects([&] { f.queue.complete_callback(signal->token + 1); }, "Wrong callback token");
        rejects([&] { f.queue.erase(cb); }, "Deleted in-flight callback");
        f.queue.run(16); check(f.queue.list(cb).commands == 2, "SIGNAL wait bypassed callback return");
        f.queue.complete_callback(signal->token); f.queue.run(16);
        auto finish = f.queue.take_callback();
        check(finish && finish->finish && finish->value == 12 && finish->function == code + 4 &&
              finish->argument == 0xFFFFFFFFu && finish->token != signal->token, "FINISH ordering/identity");
        check(f.queue.list(cb).status == GeStatus::Callback, "Completed before FINISH delivery");
        f.queue.complete_callback(finish->token);
        check(f.queue.list(cb).status == GeStatus::Complete, "FINISH callback completion"); f.queue.erase(cb);
        cb = f.queue.enqueue(base, base + 16, 0, 0); f.queue.run(2);
        f.callback(true); f.callback();
        check(!f.queue.take_callback() && f.queue.list(cb).status == GeStatus::Failed, "Stale callback slot reused");
        f.queue.erase(cb);
        f.words({0x10080000u, 0x0A004010u, 0x0F000000u, 0x0C000000u, 0x0B000000u});
        const auto call = f.queue.enqueue(base, base + 20); f.queue.run(16);
        check(f.queue.list(call).status == GeStatus::Complete && f.queue.list(call).returns.empty(), "Bounded CALL/RET");
        f.queue.erase(call);
        for (auto bad : {0x04000000u, 0x0C000000u, 0x0B000000u, 0x0E020001u, 0x08FFFFFFu}) {
            f.words({bad}); const auto failed = f.queue.enqueue(base, base + 4); f.queue.run(8);
            check(f.queue.list(failed).status == GeStatus::Failed && !f.queue.list(failed).error.empty(),
                  "Unsupported draw or control flow accepted"); f.queue.erase(failed);
        }
        f.words({0x10080000u, 0x08004000u});
        const auto loop = f.queue.enqueue(base, base + 8); f.queue.run(65536); f.queue.run(1);
        check(f.queue.list(loop).status == GeStatus::Failed, "Unbounded jump loop"); f.queue.erase(loop);
        f.words({0x0F000000u, 0x0C000000u});
        const auto first = f.queue.enqueue(base, base + 8, base);
        rejects([&] { f.queue.enqueue(base, base + 8); }, "Overlapping live ownership");
        for (std::uint32_t i = 1; i < 64; ++i) f.queue.enqueue(base + i * 8, base + i * 8 + 8);
        rejects([&] { f.queue.enqueue(base + 512, base + 520); }, "Unbounded GE list ownership");
        f.queue.run(16); check(f.queue.list(first + 1).commands == 0, "FIFO passed stalled head");
        callback_execution();
        std::cout << "Synthetic bounded GE control-stream contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
