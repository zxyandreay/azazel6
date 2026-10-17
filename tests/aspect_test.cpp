#include "azazel6/aspect.hpp"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

void equal(double actual, double expected) {
    if (std::abs(actual - expected) > 1e-9) throw std::runtime_error("Unexpected geometry");
}
template<class F> void rejects(F function) {
    try { function(); } catch (const std::invalid_argument&) { return; }
    throw std::runtime_error("Expected rejection");
}
int main() {
    using namespace azazel6;
    try {
        AspectTransform stock(16.0 / 9.0), wide(20.0 / 9.0);
        for (auto owner : {HudOwner::left, HudOwner::center, HudOwner::right}) {
            equal(stock.x(123.0, owner), 123.0);
        }
        equal(wide.x(100, HudOwner::left), 80);
        equal(wide.x(100, HudOwner::center), 128);
        equal(wide.x(380, HudOwner::right), 400);
        equal(wide.width(100), 80);
        equal(wide.x(0, HudOwner::left), 0);
        equal(wide.x(240, HudOwner::center), 240);
        equal(wide.x(480, HudOwner::right), 480);
        for (double aspect : {1.0, 4.0 / 3.0, 16.0 / 9.0, 21.0 / 9.0, 4.0}) {
            AspectTransform transform(aspect);
            equal(transform.x(75, HudOwner::left) + transform.x(405, HudOwner::right), 480);
        }
        for (double invalid : {0.0, 0.99, 4.01, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
            rejects([&] { AspectTransform unused(invalid); });
        }
        rejects([&] { (void)wide.width(-1); });
        rejects([&] { (void)wide.x(0, static_cast<HudOwner>(99)); });
        rejects([&] { (void)wide.x(std::numeric_limits<double>::infinity(), HudOwner::left); });
        std::cout << "Aspect math contracts passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
