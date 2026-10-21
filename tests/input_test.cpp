#include "azazel6/input.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>

void check(bool value) { if (!value) throw std::runtime_error("Input contract failed"); }
int main() {
    using namespace azazel6;
    try {
        check(analog_axis(-32768) == 0 && analog_axis(0) == 128 && analog_axis(32767) == 255);
        InputState input;
        auto touch = [&](std::uint64_t device, std::uint64_t finger, float x, float y, bool begin = true) {
            return input.touch(device, finger, x, y, 10, 20, 960, 600, begin);
        };
        check(touch(1, 9, 490, 320)); // Letterbox in safe rectangle.
        check(input.touches[0]->x == 240 && input.touches[0]->y == 136);
        check(touch(2, 9, 10, 48)); // Same finger ID on another device.
        check(input.touches[1]->x == 0 && input.touches[1]->y == 0);
        check(!touch(1, 100, 490, 320, false));
        check(!touch(1, 9, 490, 47, false)); // Entering letterbox cancels.
        check(!input.touches[0] && input.touches[1]);
        input.release(1, 9);
        check(input.touches[1].has_value());
        check(!touch(3, 1, 970, 320) && !touch(3, 1, 490, 592));
        check(!touch(3, 1, std::numeric_limits<float>::quiet_NaN(), 320));
        check(!input.touch(3, 1, 1, 1, 0, 0, 0, 1, true));
        input.clear();
        for (unsigned i = 0; i < 16; ++i) check(touch(1, i, 490, 320));
        check(!touch(1, 17, 490, 320));
        check(touch(1, 15, 492, 320, false));
        check(input.touches[15]->x == 241);
        input.release(1, 8);
        check(touch(1, 17, 490, 320) && input.touches[8]->finger == 17);
        input.pad = {cross | start, 0, 255};
        input.clear();
        check(input.pad.buttons == 0 && input.pad.x == 128 && input.pad.y == 128);
        for (const auto& contact : input.touches) check(!contact);
        std::cout << "Host input ownership and stock coordinate contracts passed\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
