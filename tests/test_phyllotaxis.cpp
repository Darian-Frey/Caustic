#include <doctest/doctest.h>

#include <cmath>

#include <caustic/generators/phyllotaxis.hpp>

using caustic::phyllotaxis_chord;
using caustic::golden_angle_rad;

namespace {
constexpr double kEps = 1e-9;
}

TEST_CASE("phyllotaxis_chord produces N chords") {
    const auto chords = phyllotaxis_chord(500, golden_angle_rad(), 2.0);
    CHECK(chords.size() == 500);
}

TEST_CASE("phyllotaxis_chord first point is at origin (i=0 → r=0)") {
    const auto chords = phyllotaxis_chord(10, golden_angle_rad(), 2.0);
    REQUIRE(chords.size() == 10);
    CHECK(std::abs(chords[0].a.x) < kEps);
    CHECK(std::abs(chords[0].a.y) < kEps);
}

TEST_CASE("phyllotaxis_chord radii grow as √i") {
    const auto chords = phyllotaxis_chord(20, golden_angle_rad(), 2.0);
    REQUIRE(chords.size() == 20);
    for (int i = 1; i < 20; ++i) {
        const double r = std::sqrt(chords[i].a.x * chords[i].a.x +
                                    chords[i].a.y * chords[i].a.y);
        CHECK(std::abs(r - std::sqrt(static_cast<double>(i))) < 1e-6);
    }
}

TEST_CASE("phyllotaxis_chord k=0 → every chord ends at P_0 (origin)") {
    const auto chords = phyllotaxis_chord(50, golden_angle_rad(), 0.0);
    for (const auto& c : chords) {
        CHECK(std::abs(c.b.x) < kEps);
        CHECK(std::abs(c.b.y) < kEps);
    }
}

TEST_CASE("phyllotaxis_chord with N=0 returns empty") {
    const auto chords = phyllotaxis_chord(0, golden_angle_rad(), 2.0);
    CHECK(chords.empty());
}

TEST_CASE("golden_angle_rad is ≈ 137.508°") {
    const double deg = golden_angle_rad() * 180.0 / 3.14159265358979323846;
    CHECK(std::abs(deg - 137.5077640500378) < 1e-6);
}
