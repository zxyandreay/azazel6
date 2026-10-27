#include "azazel6/clock_conversion.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

int main() {
    try {
        constexpr std::uint32_t stack = 0x08001000;
        auto rt = std::make_unique<psprecomp::Runtime>();
        auto& c = rt->cpu();
        c.gpr[4] = 1000000; c.gpr[5] = 0; c.gpr[6] = 0; c.gpr[7] = stack;
        azazel6::convert_sysclock_wide(*rt, c, stack, 8);
        if (rt->stopped() || c.gpr[2] || rt->memory().load32(stack) != 1000000)
            throw std::runtime_error("Null-first clock output lost the whole low word");
        c.gpr[4] = c.gpr[5] = 0xFFFFFFFFu; c.gpr[6] = stack; c.gpr[7] = stack + 4;
        azazel6::convert_sysclock_wide(*rt, c, stack, 8);
        if (rt->stopped() || c.gpr[2] || rt->memory().load32(stack) != 0xF7A0B5EDu ||
            rt->memory().load32(stack + 4) != 0x00086ABFu)
            throw std::runtime_error("Unsigned wide clock quotient/remainder mismatch");
        c.gpr[7] = stack + 8; c.gpr[2] = 42;
        azazel6::convert_sysclock_wide(*rt, c, stack, 8);
        if (!rt->stopped() || c.gpr[2] != 42 || rt->memory().load32(stack) != 0xF7A0B5EDu)
            throw std::runtime_error("Invalid second output partially changed state");
        rt = std::make_unique<psprecomp::Runtime>();
        auto& narrow = rt->cpu();
        rt->memory().store32(stack, 0xFFFFFFFFu);
        rt->memory().store32(stack + 4, 0xFFFFFFFFu);
        narrow.gpr[4] = stack; narrow.gpr[5] = stack + 8; narrow.gpr[6] = stack + 12;
        azazel6::convert_sysclock(*rt, narrow, stack, 16);
        if (rt->stopped() || narrow.gpr[2] || rt->memory().load32(stack + 8) != 0xF7A0B5EDu ||
            rt->memory().load32(stack + 12) != 0x00086ABFu)
            throw std::runtime_error("Pointer clock quotient/remainder mismatch");
        narrow.gpr[5] = 0; narrow.gpr[6] = stack + 8;
        azazel6::convert_sysclock(*rt, narrow, stack, 16);
        if (rt->stopped() || rt->memory().load32(stack + 8) != 0xFFFFFFFFu)
            throw std::runtime_error("Pointer clock null-first conversion mismatch");
        narrow.gpr[4] = stack + 12; narrow.gpr[5] = stack + 8; narrow.gpr[6] = 0;
        narrow.gpr[2] = 42; rt->memory().store32(stack + 8, 0x12345678u);
        azazel6::convert_sysclock(*rt, narrow, stack, 16);
        if (!rt->stopped() || narrow.gpr[2] != 42 || rt->memory().load32(stack + 8) != 0x12345678u)
            throw std::runtime_error("Invalid pointer clock input partially changed state");
        std::cout << "Shared clock conversion compatibility passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
