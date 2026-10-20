#include "azazel6/ge_lists.hpp"
#include "azazel6/bootstrap.hpp"
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
void state_contracts() {
    Fixture f;
    f.words({0x10080000u, 0x13000010u, 0x01004000u, 0x02008000u,
             0x3C000000u, 0x3D3F8000u, 0x3DBF8000u, 0xD2000003u, 0x0F000000u, 0x0C000000u});
    auto id = f.queue.enqueue(base, base + 40); f.queue.run(32);
    check(f.queue.list(id).status == GeStatus::Complete && f.queue.state().writes() == 8 &&
          f.queue.state().vertex_address() == 0x08005000u && f.queue.state().index_address() == 0x08009000u &&
          f.queue.state().view_matrix_bits()[0] == 0x3F800000u &&
          f.queue.state().view_matrix_bits()[1] == 0xBF800000u &&
          f.queue.state().word(0xD2) == 3, "Retained GE register/address/float24 state");
    f.words({0x3C00000Bu, 0x3D123456u, 0x3D654321u, 0x0F000000u, 0x0C000000u});
    id = f.queue.enqueue(base, base + 20); f.queue.run(32);
    check(f.queue.list(id).status == GeStatus::Failed &&
          f.queue.state().view_matrix_bits()[11] == 0x12345600u &&
          f.queue.state().word(0x3D) == 0x123456u, "Matrix overflow mutated retained state");
    for (auto command : {0x04000003u, 0xEA000000u, 0xCB000000u, 0xC4000000u, 0xFF000000u}) {
        Fixture unsupported; unsupported.words({command, 0x0F000000u, 0x0C000000u});
        id = unsupported.queue.enqueue(base, base + 12); unsupported.queue.run(8);
        check(unsupported.queue.list(id).status == GeStatus::Failed && unsupported.queue.state().writes() == 0,
              "Draw/transfer/cache/unknown command silently accepted");
    }
    GeState address;
    address.write(0x13FFFFFFu);
    rejects([&] { address.write(0x01000000u); }, "Relative address overflow");
    check(address.writes() == 1 && address.vertex_address() == 0, "Address rejection mutated state");
    Fixture retained;
    retained.words({0x10080000u, 0x0F000000u, 0x0C000000u});
    id = retained.queue.enqueue(base, base + 12); retained.queue.run(8);
    retained.words({0x08004004u, 0x0F000000u, 0x0C000000u});
    id = retained.queue.enqueue(base, base + 12); retained.queue.run(8);
    check(retained.queue.list(id).status == GeStatus::Complete, "List did not retain shared BASE register");
    retained.words({0x13000001u, 0x08004000u});
    id = retained.queue.enqueue(base, base + 8); retained.queue.run(8);
    check(retained.queue.list(id).status == GeStatus::Failed &&
          retained.queue.list(id).commands == 1, "Unimplemented offset branch executed");
}
void hle_contracts() {
    Fixture f;
    auto threads = register_thread_hle(*f.rt, {0x08010000u, 0x08020000u, code, 0x08010000u,
        code, code + 256, record, 32, 4096});
    rejects([&] { register_ge_list_hle(*f.rt, f.callbacks, threads, {{base, base + 4}, {base, base + 8}}); },
            "Overlapping HLE leases");
    auto queue = register_ge_list_hle(*f.rt, f.callbacks, threads, {{base, base + 16}});
    f.words({0xD2000003u, 0x00000000u, 0x0F000000u, 0x0C000000u});
    auto& c = f.rt->cpu();
    c.gpr[4] = base; c.gpr[5] = 0; c.gpr[6] = 0xFFFFFFFFu; c.gpr[7] = 0; c.gpr[31] = code + 4;
    invoke_returning_import(*f.rt, c, "sceGe_user", 0xAB49E76Au);
    const auto id = c.gpr[2];
    check(!f.rt->stopped() && queue->list(id).status == GeStatus::Complete &&
          c.pc == code + 4 && queue->state().word(0xD2) == 3, "GE enqueue result/ABI/completion");
    for (auto mode : {0u, 1u}) {
        c.gpr[4] = id; c.gpr[5] = mode;
        invoke_returning_import(*f.rt, c, "sceGe_user", 0x03444EB4u);
        check(c.gpr[2] == 0 && !f.rt->stopped(), "Completed-list sync");
    }
    c.gpr[4] = base + 4; c.gpr[5] = 0; c.gpr[6] = 0xFFFFFFFFu; c.gpr[7] = 0; c.gpr[2] = 42;
    f.rt->invoke_import("sceGe_user", 0xAB49E76Au, c);
    check(f.rt->stopped() && queue->size() == 1 && c.gpr[2] == 42, "Unknown producer mutated queue/returned success");
    Fixture bad;
    auto bad_threads = register_thread_hle(*bad.rt, {0x08010000u, 0x08020000u, code, 0x08010000u,
        code, code + 256, record, 32, 4096});
    auto bad_queue = register_ge_list_hle(*bad.rt, bad.callbacks, bad_threads, {{base, base + 12}});
    bad.words({0x04000001u, 0x0F000000u, 0x0C000000u});
    auto& b = bad.rt->cpu(); b.gpr[4] = base; b.gpr[5] = 0; b.gpr[6] = 0xFFFFFFFFu; b.gpr[7] = 0; b.gpr[2] = 42;
    bad.rt->invoke_import("sceGe_user", 0xAB49E76Au, b);
    check(bad.rt->stopped() && b.gpr[2] == 42 && bad_queue->list(1).status == GeStatus::Failed,
          "Failed GE work returned success");
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
        state_contracts();
        hle_contracts();
        std::cout << "Synthetic bounded GE control-stream contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
