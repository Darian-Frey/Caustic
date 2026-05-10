#include <doctest/doctest.h>

#include <cmath>

#include <caustic/indexer.hpp>
#include <caustic/vec2.hpp>

using caustic::Indexer;
using caustic::indexer_value;
using caustic::Vec2;

namespace {
constexpr double kEps = 1e-12;
}

TEST_CASE("ChordIndex returns i / (N-1)") {
    CHECK(indexer_value(Indexer::ChordIndex, {0,0}, {1,0}, 0,  4, 0.0) == 0.0);
    CHECK(indexer_value(Indexer::ChordIndex, {0,0}, {1,0}, 3,  4, 0.0) == 1.0);
    CHECK(indexer_value(Indexer::ChordIndex, {0,0}, {1,0}, 1,  4, 0.0) == doctest::Approx(1.0 / 3.0));
}

TEST_CASE("ChordIndex returns 0 for N <= 1") {
    CHECK(indexer_value(Indexer::ChordIndex, {0,0}, {1,0}, 0, 1, 0.0) == 0.0);
    CHECK(indexer_value(Indexer::ChordIndex, {0,0}, {1,0}, 0, 0, 0.0) == 0.0);
}

TEST_CASE("ChordLength normalises by max_length") {
    // segment of length sqrt(2) over a max of sqrt(2) → t = 1
    CHECK(indexer_value(Indexer::ChordLength, {0,0}, {1,1}, 0, 1, std::sqrt(2.0))
          == doctest::Approx(1.0));
    // segment of length 1 over max of 2 → t = 0.5
    CHECK(indexer_value(Indexer::ChordLength, {0,0}, {1,0}, 0, 1, 2.0)
          == doctest::Approx(0.5));
}

TEST_CASE("ChordLength returns 0 when max_length is 0") {
    CHECK(indexer_value(Indexer::ChordLength, {0,0}, {1,0}, 0, 1, 0.0) == 0.0);
}

TEST_CASE("Angle indexer maps 0 rad → 0.5 (centre of [0, 1])") {
    // atan2(0, 1) = 0 → (0 + π) / 2π = 0.5
    CHECK(indexer_value(Indexer::Angle, {0,0}, {1,0}, 0, 1, 0.0)
          == doctest::Approx(0.5));
}

TEST_CASE("Angle indexer maps π/2 rad → 0.75") {
    // atan2(1, 0) = π/2 → (π/2 + π) / 2π = 0.75
    CHECK(indexer_value(Indexer::Angle, {0,0}, {0,1}, 0, 1, 0.0)
          == doctest::Approx(0.75));
}

TEST_CASE("CurveT behaves like ChordIndex for our uniform samplers") {
    for (std::size_t i = 0; i < 5; ++i) {
        const double a = indexer_value(Indexer::ChordIndex, {0,0}, {1,0}, i, 5, 0.0);
        const double b = indexer_value(Indexer::CurveT, {0,0}, {1,0}, i, 5, 0.0);
        CHECK(std::abs(a - b) < kEps);
    }
}
