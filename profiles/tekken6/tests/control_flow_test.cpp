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
        std::cout << "Indirect target guard contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
