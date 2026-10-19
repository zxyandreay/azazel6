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
