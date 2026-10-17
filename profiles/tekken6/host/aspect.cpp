#include "azazel6/aspect.hpp"
#include <cmath>
#include <stdexcept>

namespace azazel6 {
AspectTransform::AspectTransform(double display_aspect) {
    if (!std::isfinite(display_aspect) || display_aspect < 1.0 || display_aspect > 4.0)
        throw std::invalid_argument("Display aspect must be finite and within [1, 4]");
    scale_ = (16.0 / 9.0) / display_aspect;
}
double AspectTransform::x(double value, HudOwner owner) const {
    if (!std::isfinite(value)) throw std::invalid_argument("Non-finite HUD coordinate");
    double anchor;
    switch (owner) {
    case HudOwner::left: anchor = 0.0; break;
    case HudOwner::center: anchor = 240.0; break;
    case HudOwner::right: anchor = 480.0; break;
    default: throw std::invalid_argument("Unknown HUD owner");
    }
    return anchor + scale_ * (value - anchor);
}
double AspectTransform::width(double value) const {
    if (!std::isfinite(value) || value < 0.0) throw std::invalid_argument("Invalid HUD width");
    return value * scale_;
}
} // namespace azazel6
