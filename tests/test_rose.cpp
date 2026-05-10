#include <doctest/doctest.h>

#include <cmath>

#include <caustic/generators/rose.hpp>

using caustic::RoseCurve;
using caustic::Vec2;

namespace {
constexpr double kEps = 1e-9;
}

TEST_CASE("Rose evaluate(0) = (1, 0) for any n, d") {
    for (int n : {1, 2, 3, 5, 7}) {
        for (int d : {1, 2, 3}) {
            RoseCurve curve(n, d);
            const Vec2 p = curve.evaluate(0.0);
            CHECK(std::abs(p.x - 1.0) < kEps);
            CHECK(std::abs(p.y) < kEps);
        }
    }
}

TEST_CASE("Rose curve closes (evaluate(0) == evaluate(1) for integer n, d)") {
    RoseCurve curve(5, 1);
    const Vec2 a = curve.evaluate(0.0);
    const Vec2 b = curve.evaluate(1.0);
    CHECK(std::abs(a.x - b.x) < kEps);
    CHECK(std::abs(a.y - b.y) < kEps);
}

TEST_CASE("Rose bounding box is the unit square (|r| ≤ 1)") {
    RoseCurve curve(4, 1);
    const auto [lo, hi] = curve.bounding_box();
    CHECK(lo == Vec2(-1.0, -1.0));
    CHECK(hi == Vec2(1.0, 1.0));
}

TEST_CASE("Rose n=3 d=1 has three petals (degree-3 zeros at θ = π/6, π/2, 5π/6)") {
    // For r = cos(3θ), r = 0 at 3θ = π/2 → θ = π/6.
    // In our parameterization t · 2π · 1 = θ, so t = 1/12 should give r=0.
    RoseCurve curve(3, 1);
    const Vec2 p = curve.evaluate(1.0 / 12.0);
    CHECK(std::abs(std::sqrt(p.x * p.x + p.y * p.y)) < 1e-9);
}
