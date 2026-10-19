#include "azazel6/ge_callbacks.hpp"
#include "azazel6/bootstrap.hpp"
#include <iostream>
#include <stdexcept>

namespace {
constexpr std::uint32_t record = 0x08003000u, begin = 0x08001000u, end = 0x08002000u;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Fixture {
    std::unique_ptr<psprecomp::Runtime> rt = std::make_unique<psprecomp::Runtime>();
    std::shared_ptr<azazel6::GeCallbacks> state = azazel6::register_ge_callback_hle(*rt, begin, end);
    Fixture() {
        rt->memory().store32(record, begin); rt->memory().store32(record + 4, 0xFFFFFFFFu);
        rt->memory().store32(record + 8, end - 4); rt->memory().store32(record + 12, 0x88000000u);
    }
    std::uint32_t call(std::uint32_t nid, std::uint32_t argument) {
        auto& c = rt->cpu(); c.gpr[4] = argument; c.gpr[2] = 0xDEADBEEFu;
        c.pc = begin; c.gpr[31] = begin + 4;
        azazel6::invoke_returning_import(*rt, c, "sceGe_user", nid); return c.gpr[2];
    }
};
constexpr auto set = 0xA4FC06A4u, unset = 0x05DB22CEu;
}
int main() {
    try {
        Fixture f;
        for (std::uint32_t i = 0; i < 16; ++i) check(f.call(set, record) == i, "Lowest free callback slot");
        check(f.call(set, record) == 0x80000022u && f.state->count() == 16, "Capacity changed registrations");
        f.rt->memory().store32(record, 0); f.rt->memory().store32(record + 8, 0);
        check(f.state->slots[0]->signal_function == begin && f.state->slots[0]->finish_function == end - 4 &&
              f.state->slots[0]->signal_argument == 0xFFFFFFFFu &&
              f.state->slots[0]->finish_argument == 0x88000000u, "Record not copied or opaque arguments changed");
        check(f.call(unset, 5) == 0 && f.call(unset, 5) == 0 && f.state->count() == 15, "Unset empty slot");
        check(f.call(set, record) == 5 && f.state->slots[5]->signal_function == 0 &&
              f.state->slots[5]->finish_function == 0 && f.rt->cpu().pc == begin + 4, "Null callbacks/reuse/return ABI");
        for (auto id : {16u, 0xFFFFFFFFu, 0x50000000u})
            check(f.call(unset, id) == 0x80000100u && f.state->count() == 16, "Invalid GE ID accepted");
        Fixture isolated; check(isolated.state->count() == 0, "Callback registrations leaked between runtimes");
        for (auto address : {0u, record + 1, 0x09FFFFF4u, 0xFFFFFFFFu, 0x88003000u}) {
            Fixture bad;
            check(bad.call(set, address) == 0xDEADBEEFu && bad.rt->stopped() && bad.state->count() == 0 &&
                  bad.rt->cpu().pc == begin, "Bad record mutated state or returned");
        }
        for (auto function : {begin - 4, begin + 1, end, 0xFFFFFFFFu, 0x88001000u}) {
            Fixture bad; bad.rt->memory().store32(record + 8, function);
            check(bad.call(set, record) == 0xDEADBEEFu && bad.rt->stopped() && bad.state->count() == 0,
                  "Bad second callback partly registered the first");
        }
        for (auto bounds : {std::pair{0u, end}, std::pair{end, begin}, std::pair{begin + 1, end},
                            std::pair{begin, 0x0A000004u}}) {
            auto rt = std::make_unique<psprecomp::Runtime>(); bool rejected = false;
            try { azazel6::register_ge_callback_hle(*rt, bounds.first, bounds.second); }
            catch (const std::exception&) { rejected = true; }
            check(rejected, "Invalid executable range accepted");
        }
        std::cout << "GE callback registration contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
