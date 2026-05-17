#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

#include <caustic/generators/lissajous_chord.hpp>
#include <caustic/generators/maurer_rose.hpp>
#include <caustic/generators/superformula_chord.hpp>

using namespace caustic;

TEST_CASE("maurer_rose emits samples+1 points (closure)") {
    const auto pts = maurer_rose(7, 71, 360);
    CHECK(pts.size() == 361);
    // Each point should lie within the unit disk (|r| ≤ 1).
    for (const auto& p : pts) {
        CHECK(p.x * p.x + p.y * p.y <= 1.0 + 1e-9);
    }
}

TEST_CASE("maurer_rose handles bad inputs gracefully") {
    CHECK(maurer_rose(0, 71, 360).empty());
    CHECK(maurer_rose(7, 71, 1).empty());
    CHECK(maurer_rose(-3, 71, 360).empty());
}

TEST_CASE("lissajous_chord returns N chords with valid t_along sweep") {
    const auto cs = lissajous_chord(1.0, 1.0, 3.0, 2.0,
                                     std::numbers::pi / 2.0,
                                     /*N=*/100, /*k=*/2.0);
    CHECK(cs.size() == 100);
    for (std::size_t i = 0; i < cs.size(); ++i) {
        CHECK(cs[i].t_along >= 0.0);
        CHECK(cs[i].t_along < 1.0);
    }
}

TEST_CASE("lissajous_chord k=2 produces cardioid-style chord pattern") {
    // For N=100 and k=2, chord i connects sample i to sample (2i mod 100).
    const auto cs = lissajous_chord(1.0, 1.0, 1.0, 1.0,
                                     0.0, 100, 2.0);
    REQUIRE(cs.size() == 100);
    // Chord 0 should be (sample 0, sample 0) — degenerate.
    CHECK(std::abs(cs[0].a.x - cs[0].b.x) < 1e-9);
    CHECK(std::abs(cs[0].a.y - cs[0].b.y) < 1e-9);
}

TEST_CASE("superformula_chord with default (starfish) params") {
    const auto cs = superformula_chord(/*m=*/5.0, /*n1=*/2.0,
                                        /*n2=*/7.0, /*n3=*/7.0,
                                        1.0, 1.0, /*N=*/144, /*k=*/2.0);
    CHECK(cs.size() == 144);
    // All points should be finite.
    for (const auto& c : cs) {
        CHECK(std::isfinite(c.a.x));
        CHECK(std::isfinite(c.a.y));
        CHECK(std::isfinite(c.b.x));
        CHECK(std::isfinite(c.b.y));
    }
}

TEST_CASE("superformula_chord with circle params (m=0) produces a circle") {
    // m=0 collapses the angular term so r ≈ const → all points on a circle.
    const auto cs = superformula_chord(/*m=*/0.0, 2.0, 2.0, 2.0,
                                        1.0, 1.0, /*N=*/72, /*k=*/3.0);
    REQUIRE(cs.size() == 72);
    // All sample radii should be approximately equal (it's a circle).
    const double r0 = std::sqrt(cs[0].a.x * cs[0].a.x + cs[0].a.y * cs[0].a.y);
    for (const auto& c : cs) {
        const double r = std::sqrt(c.a.x * c.a.x + c.a.y * c.a.y);
        CHECK(std::abs(r - r0) < 1e-6);
    }
}
