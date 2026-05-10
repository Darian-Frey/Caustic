#include <doctest/doctest.h>

#include <cmath>

#include <caustic/generators/modular_chord.hpp>

using caustic::modular_chord;

namespace {
constexpr double kEps = 1e-9;
}

TEST_CASE("modular_chord produces N chords") {
    const auto chords = modular_chord(200, 2.0);
    CHECK(chords.size() == 200);
}

TEST_CASE("modular_chord endpoints lie on the unit circle") {
    const auto chords = modular_chord(50, 2.0);
    for (const auto& c : chords) {
        const double a_r2 = c.a.x * c.a.x + c.a.y * c.a.y;
        const double b_r2 = c.b.x * c.b.x + c.b.y * c.b.y;
        CHECK(std::abs(a_r2 - 1.0) < kEps);
        CHECK(std::abs(b_r2 - 1.0) < kEps);
    }
}

TEST_CASE("modular_chord t_along is i / N") {
    const auto chords = modular_chord(10, 2.0);
    REQUIRE(chords.size() == 10);
    for (std::size_t i = 0; i < chords.size(); ++i) {
        CHECK(std::abs(chords[i].t_along - static_cast<double>(i) / 10.0) < kEps);
    }
}

TEST_CASE("modular_chord with N=4 k=2 connects expected vertices") {
    // P_0 = (1, 0); P_1 = (0, 1); P_2 = (-1, 0); P_3 = (0, -1)
    // chord_i: P_i → P_{2i mod 4}
    //   i=0: P_0 → P_0      (degenerate)
    //   i=1: P_1 → P_2
    //   i=2: P_2 → P_0
    //   i=3: P_3 → P_2
    const auto chords = modular_chord(4, 2.0);
    REQUIRE(chords.size() == 4);

    CHECK(std::abs(chords[1].a.x) < kEps);
    CHECK(std::abs(chords[1].a.y - 1.0) < kEps);
    CHECK(std::abs(chords[1].b.x + 1.0) < kEps);
    CHECK(std::abs(chords[1].b.y) < kEps);

    CHECK(std::abs(chords[2].a.x + 1.0) < kEps);
    CHECK(std::abs(chords[2].a.y) < kEps);
    CHECK(std::abs(chords[2].b.x - 1.0) < kEps);
    CHECK(std::abs(chords[2].b.y) < kEps);
}

TEST_CASE("modular_chord handles k=0 (every chord ends at P_0)") {
    const auto chords = modular_chord(8, 0.0);
    REQUIRE(chords.size() == 8);
    for (const auto& c : chords) {
        CHECK(std::abs(c.b.x - 1.0) < kEps);
        CHECK(std::abs(c.b.y) < kEps);
    }
}
