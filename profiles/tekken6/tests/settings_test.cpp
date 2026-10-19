#include "azazel6/settings.hpp"
#include "azazel6/bootstrap.hpp"
#include <iostream>
#include <stdexcept>

using namespace azazel6;
namespace {
constexpr std::uint32_t begin = 0x08010000u, end = 0x08020000u;
constexpr std::uint32_t get_nid = 0x24FD7BCFu, set_nid = 0x36AA6E91u;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
void call(psprecomp::Runtime& rt, std::uint32_t nid, std::uint32_t first, std::uint32_t second) {
    auto& c = rt.cpu(); c.gpr[4] = first; c.gpr[5] = second;
    c.gpr[2] = 0xDEADBEEFu; c.gpr[31] = 0x08001800u; c.pc = 0x08001900u;
    invoke_returning_import(rt, c, "sceImpose", nid);
}
void roundtrip() {
    auto rt = std::make_unique<psprecomp::Runtime>();
    const auto state = register_language_hle(*rt, {1, 1, 50, 60}, begin, end);
    check(state->reads == 0 && state->writes == 0, "Diagnostics not reset");
    call(*rt, get_nid, begin, end - 4);
    check(rt->memory().load32(begin) == 1 && rt->memory().load32(end - 4) == 1 &&
        rt->cpu().gpr[2] == 0 && rt->cpu().pc == 0x08001800u, "Profile defaults/return ABI");
    const auto registers = rt->cpu().gpr;
    rt->invoke_import("sceImpose", get_nid, rt->cpu());
    check(rt->cpu().gpr == registers, "Getter register clobber");
    for (std::uint32_t language = 0; language <= 11; ++language) {
        for (auto button : {0u, 1u}) {
            call(*rt, set_nid, language, button);
            call(*rt, get_nid, begin, begin + 4);
            check(!rt->stopped() && rt->memory().load32(begin) == language &&
                rt->memory().load32(begin + 4) == button && rt->cpu().gpr[2] == 0,
                "Settings did not roundtrip");
        }
    }
    call(*rt, set_nid, 3, 0);
    call(*rt, get_nid, begin, begin);
    check(rt->memory().load32(begin) == 0 && state->reads == 27 && state->writes == 25,
        "Aliased output order/diagnostics");
    auto other = std::make_unique<psprecomp::Runtime>();
    const auto isolated = register_language_hle(*other, {1, 1}, begin, end);
    check(isolated->language == 1 && isolated->confirm_button == 1 && state->language == 3,
        "Settings leaked between runtimes");
}
void failure_atomicity() {
    for (auto bad : {0u, begin - 4, end, begin + 1, 0xFFFFFFFFu, 0x88010000u}) {
        for (bool first : {false, true}) {
            auto rt = std::make_unique<psprecomp::Runtime>();
            const auto state = register_language_hle(*rt, {1, 1}, begin, end);
            rt->memory().store32(begin, 0x11223344u);
            call(*rt, get_nid, first ? bad : begin, first ? begin : bad);
            check(rt->stopped() && rt->cpu().pc == 0x08001900u && rt->cpu().gpr[2] == 0xDEADBEEFu &&
                rt->memory().load32(begin) == 0x11223344u && !state->reads && !state->writes,
                "Invalid output partially changed memory/state or returned success");
        }
    }
    for (auto [language, button] : {std::pair{12u, 1u}, {0xFFFFFFFFu, 0u}, {1u, 2u}, {0u, 0xFFFFFFFFu}}) {
        auto rt = std::make_unique<psprecomp::Runtime>();
        const auto state = register_language_hle(*rt, {1, 1}, begin, end);
        call(*rt, set_nid, language, button);
        check(rt->stopped() && state->language == 1 && state->confirm_button == 1 && !state->writes &&
            rt->cpu().gpr[2] == 0xDEADBEEFu, "Invalid settings mutated state");
    }
    auto rt = std::make_unique<psprecomp::Runtime>();
    const auto state = register_language_hle(*rt, {1, 1}, begin, end);
    rt->register_function(begin + 4, [](auto&, auto&) {}, "synthetic_code");
    rt->memory().store32(begin, 0x11223344u); call(*rt, get_nid, begin, begin + 4);
    check(rt->stopped() && !state->reads && rt->memory().load32(begin) == 0x11223344u,
        "Getter overwrote registered code");
}
void configuration() {
    auto rt = std::make_unique<psprecomp::Runtime>();
    for (auto settings : {LanguageMode{12, 1}, LanguageMode{1, 2}}) {
        bool rejected = false;
        try { register_language_hle(*rt, settings, begin, end); }
        catch (const std::exception&) { rejected = true; }
        check(rejected, "Invalid initial settings accepted");
    }
    for (auto [lo, hi] : {std::pair{begin + 1, end}, {begin, end - 1}, {end, begin},
                          {0u, end}, {begin, 0xFFFFFFFFu}}) {
        bool rejected = false;
        try { register_language_hle(*rt, {1, 1}, lo, hi); }
        catch (const std::exception&) { rejected = true; }
        check(rejected, "Invalid writable range accepted");
    }
}
}
int main() {
    try { roundtrip(); failure_atomicity(); configuration(); std::cout << "Language mode contracts passed\n"; }
    catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
