#include "azazel6/utility_modules.hpp"
#include "azazel6/bootstrap.hpp"
#include <iostream>
#include <stdexcept>

namespace {
constexpr std::uint32_t nid = 0x2A2B3DE0u;
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void load(psprecomp::Runtime& rt, std::uint32_t id) {
    auto& cpu = rt.cpu();
    cpu.gpr[4] = id;
    cpu.gpr[2] = 0xDEADBEEFu;
    cpu.gpr[31] = 0x08010004u;
    cpu.pc = 0x08010000u;
    azazel6::invoke_returning_import(rt, cpu, "sceUtility", nid);
}
}
int main() {
    try {
        psprecomp::Runtime rt;
        const auto state = azazel6::register_startup_av_modules_hle(rt);
        for (auto id : {0x301u, 0x300u, 0x302u}) {
            load(rt, id);
            check(!rt.stopped() && rt.cpu().gpr[2] == 0 &&
                  rt.cpu().pc == 0x08010004u && state->loaded(id),
                  "Checked AV module load failed");
        }
        check(state->loaded_mask == 7u && state->successful_loads == 3u,
              "AV module state incorrect");
        load(rt, 0x300u);
        check(!rt.stopped() && rt.cpu().gpr[2] == 0x80111102u &&
              state->successful_loads == 3u && state->duplicate_loads == 1u,
              "Duplicate module result incorrect");
        psprecomp::Runtime isolated;
        const auto other = azazel6::register_startup_av_modules_hle(isolated);
        check(other->loaded_mask == 0u && other->successful_loads == 0u,
              "AV module state leaked between runtimes");
        for (auto id : {0u, 0x2FFu, 0x303u, 0xFFFFFFFFu}) {
            psprecomp::Runtime invalid;
            const auto fresh = azazel6::register_startup_av_modules_hle(invalid);
            load(invalid, id);
            check(invalid.stopped() && invalid.cpu().gpr[2] == 0xDEADBEEFu &&
                  invalid.cpu().pc == 0x08010000u && fresh->loaded_mask == 0u,
                  "Unknown AV module did not fail closed");
        }
        std::cout << "Synthetic startup AV module contracts passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
