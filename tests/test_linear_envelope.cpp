#include <doctest/doctest.h>

#include <cmath>

#include <caustic/generators/linear_envelope.hpp>

using caustic::linear_envelope;
using caustic::Vec2;

namespace {
constexpr double kEps = 1e-9;
}

TEST_CASE("linear_envelope produces N chords") {
    const auto chords = linear_envelope({0, 0}, {0, 1}, {0, 0}, {1, 0}, 30, 1.0);
    CHECK(chords.size() == 30);
}

TEST_CASE("linear_envelope with N <= 0 returns empty") {
    CHECK(linear_envelope({0, 0}, {0, 1}, {0, 0}, {1, 0}, 0, 1.0).empty());
}

TEST_CASE("linear_envelope corner fan: each chord lies on x+y=const") {
    // Vertical line A (0,0)->(0,1) paired with horizontal line B (0,0)->(1,0).
    // With k=1, chord_i connects (0, i/(N-1)) to (i/(N-1), 0).
    // Every point on chord_i satisfies x + y = i/(N-1) — the parabolic
    // corner-fan envelope.
    const int N = 20;
    const auto chords = linear_envelope({0, 0}, {0, 1}, {0, 0}, {1, 0}, N, 1.0);
    REQUIRE(chords.size() == static_cast<std::size_t>(N));
    for (std::size_t i = 0; i < chords.size(); ++i) {
        const double sum_a = chords[i].a.x + chords[i].a.y;
        const double sum_b = chords[i].b.x + chords[i].b.y;
        CHECK(std::abs(sum_a - sum_b) < kEps);
    }
}

TEST_CASE("linear_envelope endpoint indices: i=0 starts at a_start, i=N-1 ends at a_end") {
    const auto chords = linear_envelope({-1, 0}, {1, 0}, {0, -1}, {0, 1}, 10, 1.0);
    REQUIRE(chords.size() == 10);
    CHECK(std::abs(chords[0].a.x + 1.0) < kEps);    // i=0 -> a_start = (-1, 0)
    CHECK(std::abs(chords[9].a.x - 1.0) < kEps);    // i=9 -> a_end = (1, 0)
}

TEST_CASE("linear_envelope k=0 collapses every chord's B endpoint to b_start") {
    const auto chords = linear_envelope({0, 0}, {0, 1}, {1, 0}, {2, 0}, 15, 0.0);
    for (const auto& c : chords) {
        CHECK(std::abs(c.b.x - 1.0) < kEps);
        CHECK(std::abs(c.b.y) < kEps);
    }
}

TEST_CASE("linear_envelope bowtie (parallel lines, k=-1) crosses through middle") {
    // Left vertical line (-1, -1)->(-1, 1) and right vertical line (1, -1)->(1, 1).
    // With k=-1, chord_i connects A_i to B_{N-i mod N} which reverses B.
    // The chord at i = N/2 crosses through y=0 (the centre row).
    const int N = 10;
    const auto chords = linear_envelope({-1, -1}, {-1, 1}, {1, -1}, {1, 1}, N, -1.0);
    REQUIRE(chords.size() == static_cast<std::size_t>(N));
    // i=0 starts at (-1, -1), ends at B's round(0)=0 → (1, -1). A line along
    // the bottom edge — horizontal.
    CHECK(std::abs(chords[0].a.y + 1.0) < kEps);
    CHECK(std::abs(chords[0].b.y + 1.0) < kEps);
    // i=1 starts at A_1; ends at B_{N-1} = (1, 1). A diagonal crossing.
    CHECK(chords[1].a.y < chords[1].b.y);  // upward diagonal
}
