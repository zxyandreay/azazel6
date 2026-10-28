#include "azazel6/bootstrap.hpp"
#include "azazel6/power.hpp"
#include <iostream>
#include <stdexcept>

using psprecomp::Runtime;
using namespace azazel6;

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void nominal_query_contracts() {
    auto rt = std::make_unique<Runtime>();
    auto state = register_power_clock_hle(*rt);
    auto& ctx = rt->cpu();
    const struct Query { std::uint32_t nid; std::uint32_t expected; } queries[] = {
        {kPowerGetPllClockFrequencyIntNid, 222},
        {kPowerGetCpuClockFrequencyIntNid, 222},
        {kPowerGetBusClockFrequencyIntNid, 111},
    };
    for (const auto query : queries) {
        ctx.gpr.fill(0xA5A5A5A5u);
        ctx.gpr[0] = 0;
        ctx.pc = 0x08804000u;
        ctx.gpr[31] = 0x08805000u;
        const auto before = ctx.gpr;
        invoke_returning_import(*rt, ctx, "scePower", query.nid);
        require(!rt->stopped() && ctx.pc == 0x08805000u && ctx.gpr[2] == query.expected,
                "Power query return ABI");
        for (std::size_t i = 0; i < before.size(); ++i)
            if (i != 2) require(ctx.gpr[i] == before[i], "Power query clobbered a register");
    }
    require(state->pll_mhz == 222 && state->cpu_mhz == 222 && state->bus_mhz == 111,
            "Power query changed nominal clocks");
    require(state->pll_reads == 1 && state->cpu_reads == 1 && state->bus_reads == 1,
            "Power query accounting");
}

void runtime_ownership_contract() {
    auto first = std::make_unique<Runtime>();
    auto first_state = register_power_clock_hle(*first);
    first->invoke_import("scePower", kPowerGetPllClockFrequencyIntNid, first->cpu());
    auto second = std::make_unique<Runtime>();
    auto second_state = register_power_clock_hle(*second);
    require(first_state->pll_reads == 1 && second_state->pll_reads == 0,
            "Power state leaked across runtimes");
    first_state.reset();
    first->invoke_import("scePower", kPowerGetPllClockFrequencyIntNid, first->cpu());
    require(!first->stopped(), "Runtime did not retain power state");
}

struct CallbackFixture {
    std::unique_ptr<Runtime> rt = std::make_unique<Runtime>();
    std::shared_ptr<ThreadRegistry> threads;
    std::shared_ptr<PowerClocks> power;
    CallbackFixture() {
        threads = register_thread_hle(*rt, {0x09000000u, 0x09010000u, 0x08804000u,
            0x08C00000u, 0x08804000u, 0x08C00000u, 0x08805000u, 40u, 0x4000u});
        power = register_power_clock_hle(*rt);
        register_power_callback_hle(*rt, power, threads);
    }
    std::uint32_t create_callback(std::uint32_t index = 0) {
        auto& c = rt->cpu();
        const auto name = 0x08810000u + index * 16u;
        rt->memory().store8(name, 'p'); rt->memory().store8(name + 1, 0);
        c.gpr[4] = name; c.gpr[5] = 0x08820000u + index * 4u; c.gpr[6] = index;
        rt->invoke_import("ThreadManForUser", 0xE81CAF8Fu, c);
        require(c.gpr[2] >= 0x50000000u, "Callback setup");
        return c.gpr[2];
    }
    std::uint32_t register_callback(std::int32_t slot, std::uint32_t uid) {
        auto& c = rt->cpu();
        c.gpr[4] = static_cast<std::uint32_t>(slot); c.gpr[5] = uid;
        c.pc = 0x08804000u; c.gpr[31] = 0x08805000u;
        invoke_returning_import(*rt, c, "scePower", kPowerRegisterCallbackNid);
        return c.gpr[2];
    }
};

void callback_registration_contracts() {
    CallbackFixture f;
    const auto first = f.create_callback();
    require(f.register_callback(-1, first) == 0 && f.power->callback_slots[0] == first,
            "Automatic power callback slot");
    require(f.power->pending_notifications.size() == 1 &&
            f.power->pending_notifications[0].callback == first &&
            f.power->pending_notifications[0].value == 0x000010E4u,
            "Initial power callback notification");

    const auto second = f.create_callback(1);
    require(f.register_callback(5, second) == 0 && f.power->callback_slots[5] == second,
            "Explicit power callback slot");
    const auto notifications = f.power->pending_notifications.size();
    require(f.register_callback(5, first) == 0x80000020u &&
            f.power->pending_notifications.size() == notifications,
            "Occupied power callback slot mutated state");
}

void callback_rejection_contracts() {
    for (const auto slot : {-2, 16}) {
        CallbackFixture f; const auto callback = f.create_callback();
        require(f.register_callback(slot, callback) == 0x80000102u &&
                f.power->pending_notifications.empty(), "Invalid power callback slot");
    }
    for (const auto callback : {0u, 0x5000FFFFu}) {
        CallbackFixture f;
        require(f.register_callback(-1, callback) == 0x80000100u &&
                f.power->pending_notifications.empty(), "Invalid power callback UID");
    }
    {
        CallbackFixture f; const auto callback = f.create_callback();
        for (std::int32_t slot = 0; slot < 16; ++slot)
            require(f.register_callback(slot, callback) == 0, "Power callback slot fill");
        const auto notifications = f.power->pending_notifications.size();
        require(f.register_callback(-1, callback) == 0x80000022u &&
                f.power->pending_notifications.size() == notifications,
                "Full power callback table mutated state");
    }
    {
        CallbackFixture f; const auto callback = f.create_callback(); auto& c = f.rt->cpu();
        c.gpr[4] = 0; c.pc = 0x08804000u; c.gpr[31] = 0x08805000u;
        invoke_returning_import(*f.rt, c, "Kernel_Library", 0x092968F4u);
        f.register_callback(-1, callback);
        require(f.rt->stopped() && f.power->pending_notifications.empty(),
                "Disabled-interrupt registration mutated state");
    }
}

int main() {
    try {
        nominal_query_contracts(); runtime_ownership_contract();
        callback_registration_contracts(); callback_rejection_contracts();
        std::cout << "Power query and callback registration contracts passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
