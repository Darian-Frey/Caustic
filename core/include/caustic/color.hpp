#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace caustic {

// RGBA in [0, 1] doubles. Conversion to 8-bit happens at the GPU/file boundary
// in renderers; core stays in double precision per architecture invariant §2.
struct Color {
    double r;
    double g;
    double b;
    double a;

    constexpr Color() : r(0.0), g(0.0), b(0.0), a(1.0) {}
    constexpr Color(double r_, double g_, double b_, double a_ = 1.0)
        : r(r_), g(g_), b(b_), a(a_) {}

    constexpr bool operator==(const Color&) const = default;
};

// Linear interpolation between two colors. t ∈ [0, 1]; clamps outside.
constexpr Color lerp(Color a, Color b, double t) {
    t = (t < 0.0) ? 0.0 : (t > 1.0) ? 1.0 : t;
    return {
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t,
    };
}

// HSV → RGB. h in degrees [0, 360), s,v in [0, 1]. Returned alpha = 1.
inline Color hsv_to_rgb(double h, double s, double v) {
    h = std::fmod(h, 360.0);
    if (h < 0.0) h += 360.0;

    const double c = v * s;
    const double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
    const double m = v - c;

    double r = 0.0, g = 0.0, b = 0.0;
    if      (h <  60.0) { r = c; g = x; }
    else if (h < 120.0) { r = x; g = c; }
    else if (h < 180.0) { g = c; b = x; }
    else if (h < 240.0) { g = x; b = c; }
    else if (h < 300.0) { r = x; b = c; }
    else                { r = c; b = x; }

    return {r + m, g + m, b + m, 1.0};
}

}  // namespace caustic
