#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

#include <caustic/generators/lissajous.hpp>

using caustic::LissajousCurve;
using caustic::Vec2;

namespace {
constexpr double kEps = 1e-12;
}

TEST_CASE("Lissajous a=b=1 phi=0 lies on y=x") {
    LissajousCurve curve(1.0, 1.0, 1.0, 1.0, 0.0);
    for (int i = 0; i <= 50; ++i) {
        const double t = static_cast<double>(i) / 50.0;
        const Vec2 p = curve.evaluate(t);
        CHECK(std::abs(p.x - p.y) < kEps);
    }
}

TEST_CASE("Lissajous bowtie (1:2, phi=pi/2) starts at (1, 0) and crosses origin at t=0.25") {
    LissajousCurve curve(1.0, 1.0, 1.0, 2.0, std::numbers::pi / 2.0);
    const Vec2 start = curve.evaluate(0.0);
    CHECK(std::abs(start.x - 1.0) < kEps);
    CHECK(std::abs(start.y) < kEps);

    const Vec2 mid = curve.evaluate(0.25);
    CHECK(std::abs(mid.x) < kEps);
    CHECK(std::abs(mid.y) < kEps);
}

TEST_CASE("Lissajous closes at t=1 for integer frequency ratio") {
    LissajousCurve curve(1.0, 1.0, 3.0, 2.0, std::numbers::pi / 2.0);
    const Vec2 a = curve.evaluate(0.0);
    const Vec2 b = curve.evaluate(1.0);
    CHECK(std::abs(a.x - b.x) < 1e-9);
    CHECK(std::abs(a.y - b.y) < 1e-9);
}

TEST_CASE("Lissajous bounding box is (A, B)") {
    LissajousCurve curve(2.0, 3.0, 1.0, 1.0, 0.0);
    const auto [lo, hi] = curve.bounding_box();
    CHECK(lo == Vec2(-2.0, -3.0));
    CHECK(hi == Vec2(2.0, 3.0));
}
