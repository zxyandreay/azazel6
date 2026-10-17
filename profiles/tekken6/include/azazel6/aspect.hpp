#pragma once

namespace azazel6 {
enum class HudOwner { left, center, right };

// PSP logical coordinates: 480 wide. Caller must establish semantic ownership.
// SIDE_TO_CENTER health-bar composition is deliberately not an ordinary anchor.
class AspectTransform {
public:
    explicit AspectTransform(double display_aspect);
    [[nodiscard]] double x(double value, HudOwner owner) const;
    [[nodiscard]] double width(double value) const;
    [[nodiscard]] double scale() const noexcept { return scale_; }
private:
    double scale_;
};
} // namespace azazel6
