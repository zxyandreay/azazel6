#include "azazel6/control_flow.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>
int main() {
    try {
        for (auto target : {0u, 0x08804001u, 0xFFFFFFFFu, 0x88804000u, 0x08804000u, 0x08804100u}) {
            auto rt = std::make_unique<psprecomp::Runtime>();
            rt->cpu().pc = 0x08804200u; rt->cpu().gpr[31] = 0xDEADBEEFu;
            const auto before = rt->cpu();
            const auto allowed = azazel6::guard_indirect_call(*rt, 0x08804200u, target, {0x08804000u, 0x08804100u});
            const bool expected = target == 0x08804000u || target == 0x08804100u;
            if (allowed != expected || rt->stopped() == expected || rt->cpu().pc != before.pc || rt->cpu().gpr != before.gpr)
                throw std::runtime_error("Indirect guard accepted unknown target or changed guest state");
        }
        for (auto target : {0u, 0x08804000u, 0xFFFFFFFFu}) {
            auto rt = std::make_unique<psprecomp::Runtime>();
            rt->cpu().pc = 0x08804200u; rt->cpu().gpr[31] = 0xDEADBEEFu;
            const auto before = rt->cpu();
            if (azazel6::guard_indirect_call(*rt, before.pc, target, {}) || !rt->stopped() ||
                rt->cpu().pc != before.pc || rt->cpu().gpr != before.gpr)
                throw std::runtime_error("Terminal indirect trap did not preserve guest state");
        }
        // Synthetic vtable dispatch through the actual runtime. Registering an
        // alternate callee must not authorize it at this guarded call site.
        for (bool permitted : {false, true}) {
            constexpr std::uint32_t entry = 0x08804000u, continuation = entry + 8;
            constexpr std::uint32_t method = 0x08804100u, alternate = 0x08804200u;
            constexpr std::uint32_t object = 0x08002000u, table = 0x08003000u;
            auto rt = std::make_unique<psprecomp::Runtime>();
            auto& memory = rt->memory();
            memory.store32(object, table);
            memory.store32(table + 24, permitted ? method : alternate);
            memory.store32(object + 4, 0); memory.store32(object + 8, 0);
            rt->cpu().gpr[4] = object; rt->cpu().gpr[31] = 0xDEADBEEFu;
            rt->register_function(entry, [](auto& runtime, auto& c) {
                c.gpr[3] = runtime.memory().load32(c.gpr[4]);
                c.gpr[2] = runtime.memory().load32(c.gpr[3] + 24);
                if (!azazel6::guard_indirect_call(runtime, entry, c.gpr[2], {method})) return;
                const auto target = c.gpr[2];
                c.set_gpr(31, continuation);
                runtime.memory().store32(c.gpr[4] + 4, 1); // synthetic call delay effect
                c.pc = target;
            }, "synthetic_object_call");
            const auto body = [](auto& runtime, auto& c) {
                auto& data = runtime.memory();
                data.store32(c.gpr[4] + 8, data.load32(c.gpr[4] + 8) + 1);
                c.set_gpr(2, 17); c.pc = c.gpr[31];
            };
            rt->register_function(method, body, "synthetic_reviewed_method");
            rt->register_function(alternate, body, "synthetic_unreviewed_method");
            rt->register_function(continuation, [](auto& runtime, auto&) {
                runtime.stop("synthetic continuation reached");
            }, "synthetic_continuation");
            rt->run(entry, 4);
            if (permitted) {
                if (rt->cpu().gpr[2] != 17 || rt->cpu().gpr[31] != continuation ||
                    memory.load32(object + 4) != 1 || memory.load32(object + 8) != 1 ||
                    rt->stop_reason() != "synthetic continuation reached")
                    throw std::runtime_error("Reviewed object call lost delay/link/return semantics");
            } else if (rt->cpu().gpr[31] != 0xDEADBEEFu ||
                       memory.load32(object + 4) || memory.load32(object + 8) ||
                       !rt->stop_reason().starts_with("Unverified indirect target ")) {
                throw std::runtime_error("Unreviewed registered method executed or changed call state");
            }
        }
        // The pinned BREAK emitter uses this exact runtime path and returns.
        {
            auto rt = std::make_unique<psprecomp::Runtime>();
            rt->cpu().pc = 0x08804000u;
            rt->cpu().gpr[2] = 123;
            rt->cpu().hi = 17; rt->cpu().lo = 19;
            const auto before = rt->cpu();
            rt->unsupported(before.pc, 0x000001CDu, "guest break trap");
            if (!rt->stopped() || rt->cpu().pc != before.pc || rt->cpu().gpr != before.gpr ||
                rt->cpu().hi != before.hi || rt->cpu().lo != before.lo ||
                rt->stop_reason().find("guest break trap") == std::string::npos)
                throw std::runtime_error("Guest BREAK did not stop without register mutation");
        }
        std::cout << "Indirect target guard contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
