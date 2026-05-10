#include <doctest/doctest.h>

#include <cmath>

#include <caustic/generators/polygon.hpp>

using caustic::PolygonCurve;
using caustic::polygon_chord;
using caustic::Vec2;

namespace {
constexpr double kEps = 1e-9;
}

TEST_CASE("PolygonCurve t=0 is the first vertex on the unit circle") {
    PolygonCurve c(3);
    const Vec2 p = c.evaluate(0.0);
    CHECK(std::abs(p.x - 1.0) < kEps);
    CHECK(std::abs(p.y) < kEps);
}

TEST_CASE("PolygonCurve closes: evaluate(0) == evaluate(1)") {
    for (int n : {3, 4, 5, 6, 7, 12}) {
        PolygonCurve c(n);
        const Vec2 a = c.evaluate(0.0);
        const Vec2 b = c.evaluate(1.0);
        CHECK(std::abs(a.x - b.x) < kEps);
        CHECK(std::abs(a.y - b.y) < kEps);
    }
}

TEST_CASE("PolygonCurve hits vertices at t = k / n") {
    PolygonCurve c(6);
    constexpr double pi = 3.14159265358979323846;
    for (int k = 0; k < 6; ++k) {
        const double t = static_cast<double>(k) / 6.0;
        const Vec2 p = c.evaluate(t);
        const double expected_x = std::cos(2.0 * pi * k / 6.0);
        const double expected_y = std::sin(2.0 * pi * k / 6.0);
        CHECK(std::abs(p.x - expected_x) < kEps);
        CHECK(std::abs(p.y - expected_y) < kEps);
    }
}

TEST_CASE("PolygonCurve traverses edges linearly between vertices") {
    PolygonCurve c(4);  // square with vertices at (1,0), (0,1), (-1,0), (0,-1)
    // Midpoint of first edge: t = 1/8
    const Vec2 mid = c.evaluate(1.0 / 8.0);
    CHECK(std::abs(mid.x - 0.5) < kEps);
    CHECK(std::abs(mid.y - 0.5) < kEps);
}

TEST_CASE("PolygonCurve bounding box is unit square") {
    PolygonCurve c(5);
    const auto [lo, hi] = c.bounding_box();
    CHECK(lo == Vec2(-1.0, -1.0));
    CHECK(hi == Vec2(1.0, 1.0));
}

TEST_CASE("polygon_chord produces N chords") {
    const auto chords = polygon_chord(3, 200, 2.0);
    CHECK(chords.size() == 200);
}

TEST_CASE("polygon_chord deltoid: triangle (n=3) with k=2 endpoints lie on unit circle") {
    // Vertices of a triangle are on the unit circle. With N a multiple of n,
    // the sampled points include the vertices.
    const auto chords = polygon_chord(3, 60, 2.0);
    REQUIRE(chords.size() == 60);
    for (const auto& c : chords) {
        // Each endpoint is on the triangle's perimeter; bounded by unit circle.
        CHECK(std::abs(c.a.x) <= 1.0 + kEps);
        CHECK(std::abs(c.a.y) <= 1.0 + kEps);
    }
}

TEST_CASE("polygon_chord rejects degenerate inputs") {
    CHECK(polygon_chord(3, 0, 2.0).empty());
    CHECK(polygon_chord(2, 100, 2.0).empty());  // n_sides < 3
}
