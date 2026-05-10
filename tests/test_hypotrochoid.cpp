#include <doctest/doctest.h>

#include <cmath>

#include <caustic/generators/hypotrochoid.hpp>

using caustic::HypotrochoidCurve;
using caustic::Vec2;

namespace {
constexpr double kEps = 1e-9;
}

TEST_CASE("Hypotrochoid Tusi couple (R=2, r=1, d=1) is a horizontal line") {
    HypotrochoidCurve curve(2.0, 1.0, 1.0);
    for (int i = 0; i <= 100; ++i) {
        const double t = static_cast<double>(i) / 100.0;
        const Vec2 p = curve.evaluate(t);
        CHECK(std::abs(p.y) < kEps);
        // x must stay within the diameter (2(R-r)+2d when d=R-r → ±2)
        CHECK(std::abs(p.x) <= 2.0 + kEps);
    }
}

TEST_CASE("Hypotrochoid R=5 r=3 d=2 closes between t=0 and t=1") {
    HypotrochoidCurve curve(5.0, 3.0, 2.0);
    const Vec2 a = curve.evaluate(0.0);
    const Vec2 b = curve.evaluate(1.0);
    CHECK(std::abs(a.x - b.x) < kEps);
    CHECK(std::abs(a.y - b.y) < kEps);
}

TEST_CASE("Hypotrochoid bounding box is |R-r|+|d|") {
    HypotrochoidCurve curve(5.0, 3.0, 2.0);
    const auto [lo, hi] = curve.bounding_box();
    CHECK(lo == Vec2(-4.0, -4.0));
    CHECK(hi == Vec2(4.0, 4.0));
}
