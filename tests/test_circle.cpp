#include <doctest/doctest.h>

#include <cmath>

#include <caustic/circle.hpp>
#include <caustic/vec2.hpp>

using caustic::Circle;
using caustic::Vec2;

namespace {
constexpr double kEps = 1e-12;
}

TEST_CASE("Circle evaluates the unit circle at quarter points") {
    Circle c;

    const Vec2 p0 = c.evaluate(0.0);
    CHECK(std::abs(p0.x - 1.0) < kEps);
    CHECK(std::abs(p0.y) < kEps);

    const Vec2 p_quarter = c.evaluate(0.25);
    CHECK(std::abs(p_quarter.x) < kEps);
    CHECK(std::abs(p_quarter.y - 1.0) < kEps);

    const Vec2 p_half = c.evaluate(0.5);
    CHECK(std::abs(p_half.x + 1.0) < kEps);
    CHECK(std::abs(p_half.y) < kEps);
}

TEST_CASE("Circle bounding box spans the unit square") {
    Circle c;
    const auto [lo, hi] = c.bounding_box();
    CHECK(lo == Vec2(-1.0, -1.0));
    CHECK(hi == Vec2(1.0, 1.0));
}
