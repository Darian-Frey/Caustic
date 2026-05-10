#include <doctest/doctest.h>

#include <cmath>

#include <caustic/generators/epitrochoid.hpp>

using caustic::EpitrochoidCurve;
using caustic::Vec2;

namespace {
constexpr double kEps = 1e-9;
}

TEST_CASE("Epitrochoid R=r=d=1 cardioid: cusp at (1, 0)") {
    EpitrochoidCurve curve(1.0, 1.0, 1.0);
    const Vec2 cusp = curve.evaluate(0.0);
    CHECK(std::abs(cusp.x - 1.0) < kEps);
    CHECK(std::abs(cusp.y) < kEps);
}

TEST_CASE("Epitrochoid R=3 r=1 d=1.5 closes between t=0 and t=1") {
    EpitrochoidCurve curve(3.0, 1.0, 1.5);
    const Vec2 a = curve.evaluate(0.0);
    const Vec2 b = curve.evaluate(1.0);
    CHECK(std::abs(a.x - b.x) < kEps);
    CHECK(std::abs(a.y - b.y) < kEps);
}

TEST_CASE("Epitrochoid bounding box is (R+r)+|d|") {
    EpitrochoidCurve curve(3.0, 1.0, 1.5);
    const auto [lo, hi] = curve.bounding_box();
    CHECK(lo == Vec2(-5.5, -5.5));
    CHECK(hi == Vec2(5.5, 5.5));
}
