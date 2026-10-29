#include "azazel6/ge_clear.hpp"
#include "azazel6/ge_lists.hpp"
#include "azazel6/display.hpp"
#include "psprecomp/common.hpp"
#include <iostream>
#include <stdexcept>

namespace {
using namespace azazel6;
constexpr auto input = 0x08004000u, color_base = 0x04000000u, depth_base = 0x04088000u;
void check(bool ok, const char* why) { if (!ok) throw std::runtime_error(why); }
struct Fixture {
    psprecomp::GuestMemory memory;
    GeState state;
    Fixture(bool alias = false) {
        for (auto word : {0x10080000u, 0x01004000u, 0x1280011Cu, 0xD3000701u,
                          0x9C000000u, 0x9D000200u, 0x9E088000u, 0x9F000200u, 0xD50FFFFFu}) state.write(word);
        if (alias) state.write(0x9E000000u);
        for (unsigned i = 0; i < 32; ++i) {
            const auto at = input + i * 24;
            for (unsigned j = 0; j < 2; ++j) {
                const auto v = at + j * 12;
                memory.store32(v, 0xFF0000FFu);
                memory.store16(v + 4, static_cast<std::uint16_t>(j * 16));
                memory.store16(v + 6, static_cast<std::uint16_t>(j * 8));
                memory.store16(v + 8, 0xFFF0u); memory.store16(v + 10, 0xA55Au);
            }
        }
        for (unsigned y = 0; y < 16; ++y) for (unsigned x = 0; x < 512; ++x) {
            memory.store16(color_base + (y * 512 + x) * 2, 0xA55Au);
            memory.store16(depth_base + (y * 512 + x) * 2, 0x5AA5u);
        }
    }
    std::uint64_t draw(std::uint32_t count = 2, std::uint32_t end = input + 768) {
        return execute_ge_clear(memory, state, 0x04060000u | count, input, end);
    }
};
void positive_contracts() {
    for (bool alias : {false, true}) {
        Fixture f(alias);
        check(f.draw() == 128, "Clear pixel count");
        for (unsigned y = 0; y < 16; ++y) for (unsigned x = 0; x < 512; ++x) {
            const bool drawn = x < 16 && y < 8;
            check(f.memory.load16(color_base + (y * 512 + x) * 2) == (drawn ? 0x001Fu : 0xA55Au),
                  "Color packing/stride/alias ordering");
            if (!alias) check(f.memory.load16(depth_base + (y * 512 + x) * 2) == (drawn ? 0xFFF0u : 0x5AA5u),
                              "Unsigned depth/padding preservation");
        }
    }
    Fixture clipped;
    clipped.state.write(0xD4000804u); // x=4,y=2
    clipped.state.write(0xD500140Bu); // inclusive x=11,y=5
    check(clipped.draw() == 32, "Scissor intersection area");
    for (unsigned y = 0; y < 8; ++y) for (unsigned x = 0; x < 16; ++x)
        check(clipped.memory.load16(color_base + (y * 512 + x) * 2) ==
              (x >= 4 && x < 12 && y >= 2 && y < 6 ? 0x001Fu : 0xA55Au), "Scissor/pixel endpoint");
    for (auto [rgba, packed] : {std::pair{0xFF00FF00u, 0x07E0u}, std::pair{0xFFFF0000u, 0xF800u}}) {
        Fixture f; f.memory.store32(input, rgba); f.memory.store32(input + 12, rgba);
        f.draw(); check(f.memory.load16(color_base) == packed, "RGB565 channel layout");
    }
}
void failures_are_atomic() {
    for (unsigned scenario = 0; scenario < 14; ++scenario) {
        Fixture f; std::uint32_t count = 2, end = input + 768;
        switch (scenario) {
        case 0: count = 0; break;
        case 1: count = 3; break;
        case 2: count = 66; break;
        case 3: end = input + 20; break;
        case 4: f.state.write(0xD3000000u); break;
        case 5: f.state.write(0xD2000001u); break;
        case 6: f.state.write(0xE8000001u); break;
        case 7: f.state.write(0x9C1FFFF0u); break;
        case 8: f.state.write(0x9F000008u); break;
        case 9: f.state.write(0xD5FFFFFFu); break;
        case 10: f.state.write(0x01804000u); break; // outside this lease
        case 11: count = 4; f.memory.store16(input + 24 + 12 + 4, 1025); break;
        case 12: f.memory.store32(input + 12, 0); break;
        case 13: f.memory.store16(input + 12 + 8, 0); break;
        }
        const auto before = f.memory.vram_bytes();
        bool rejected = false;
        try { f.draw(count, end); } catch (const psprecomp::Error&) { rejected = true; }
        check(rejected && before == f.memory.vram_bytes(), "Failed preflight wrote VRAM or accepted bad draw");
    }
}
void queue_contract() {
    Fixture f;
    auto callbacks = std::make_shared<GeCallbacks>();
    GeQueue queue(f.memory, callbacks);
    constexpr auto commands = input + 512;
    std::uint32_t at = commands;
    for (auto word : {0x10080000u, 0x01004000u, 0x1280011Cu, 0xD3000701u,
        0x9C000000u, 0x9D000200u, 0x9E088000u, 0x9F000200u, 0xD50FFFFFu,
        0x04060002u, 0x0F000000u, 0x0C000000u}) { f.memory.store32(at, word); at += 4; }
    // Enter through a branch over vertex data, as a command buffer may contain both.
    f.memory.store32(input - 8, 0x10080000u);
    f.memory.store32(input - 4, 0x08004200u);
    const auto id = queue.enqueue(input - 8, at); queue.run(64);
    check(queue.list(id).status == GeStatus::Complete && queue.clear_draws() == 1 && queue.clear_pixels() == 128 &&
          queue.state().vertex_address() == input + 24 && queue.state().word(1) == 0x4000u,
          "Queue clear/FINISH or internal vertex-pointer advance");
    DisplayBuffers display;
    check(display.set(f.memory, {color_base, 512, 0}, 1) == 0, "GE scanout descriptor");
    display.vblank();
    const auto pixels = display.readback_rgba(f.memory);
    for (unsigned y = 0; y < 16; ++y) for (unsigned x = 0; x < 480; ++x) {
        const bool red = x < 16 && y < 8;
        const auto at = (y * 480 + x) * 4;
        check(pixels[at] == (red ? 255 : 214) && pixels[at + 1] == (red ? 0 : 170) &&
              pixels[at + 2] == (red ? 0 : 165) && pixels[at + 3] == 255,
              "Synthetic GE clear to latched RGBA readback");
    }
}
}
int main() {
    try { positive_contracts(); failures_are_atomic(); queue_contract();
        std::cout << "Synthetic checked GE clear/VRAM contracts passed\n";
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
