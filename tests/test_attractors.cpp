#include <doctest/doctest.h>

#include <cmath>

#include <caustic/generators/attractors.hpp>

using namespace caustic;

TEST_CASE("Clifford canonical butterfly stays bounded for 5k iterations") {
    auto orbit = clifford_orbit(-1.4, 1.6, 1.0, 0.7, 0.1, 0.1, 100, 5000);
    CHECK_FALSE(orbit.diverged);
    CHECK(orbit.points.size() == 5000);
    // Clifford orbits live in [-2, 2] x [-2, 2] for these parameters.
    for (const auto& p : orbit.points) {
        CHECK(std::abs(p.x) <= 2.1);
        CHECK(std::abs(p.y) <= 2.1);
    }
}

TEST_CASE("De Jong classic stays bounded") {
    auto orbit = de_jong_orbit(1.4, -2.3, 2.4, -2.1, 0.1, 0.1, 100, 5000);
    CHECK_FALSE(orbit.diverged);
    CHECK(orbit.points.size() == 5000);
    for (const auto& p : orbit.points) {
        CHECK(std::abs(p.x) <= 2.1);
        CHECK(std::abs(p.y) <= 2.1);
    }
}

TEST_CASE("Tinkerbell canonical orbit stays in its basin") {
    auto orbit = tinkerbell_orbit(0.9, -0.6013, 2.0, 0.5, -0.72, -0.64, 100, 5000);
    CHECK_FALSE(orbit.diverged);
    CHECK(orbit.points.size() == 5000);
}

TEST_CASE("Tinkerbell diverges from an out-of-basin starting point") {
    auto orbit = tinkerbell_orbit(0.9, -0.6013, 2.0, 0.5, 5.0, 5.0, 0, 10000);
    CHECK(orbit.diverged);
    // Truncated — fewer points than requested.
    CHECK(orbit.points.size() < 10000);
}

TEST_CASE("Determinism: same params + initial condition → byte-identical orbit") {
    auto a = clifford_orbit(-1.4, 1.6, 1.0, 0.7, 0.1, 0.1, 100, 1000);
    auto b = clifford_orbit(-1.4, 1.6, 1.0, 0.7, 0.1, 0.1, 100, 1000);
    REQUIRE(a.points.size() == b.points.size());
    for (std::size_t i = 0; i < a.points.size(); ++i) {
        CHECK(a.points[i].x == b.points[i].x);
        CHECK(a.points[i].y == b.points[i].y);
    }
}

TEST_CASE("Burn-in shifts the recorded starting point") {
    auto no_burn   = clifford_orbit(-1.4, 1.6, 1.0, 0.7, 0.1, 0.1,   0, 100);
    auto with_burn = clifford_orbit(-1.4, 1.6, 1.0, 0.7, 0.1, 0.1, 500, 100);
    REQUIRE(no_burn.points.size() == 100);
    REQUIRE(with_burn.points.size() == 100);
    // First recorded points differ when burn_in advances past the transient.
    const bool same_point = no_burn.points[0].x == with_burn.points[0].x
                         && no_burn.points[0].y == with_burn.points[0].y;
    CHECK_FALSE(same_point);
}

TEST_CASE("Zero iterations returns empty orbit, no divergence") {
    auto orbit = clifford_orbit(-1.4, 1.6, 1.0, 0.7, 0.1, 0.1, 0, 0);
    CHECK(orbit.points.empty());
    CHECK_FALSE(orbit.diverged);
}

TEST_CASE("iterate_orbit honours a custom step function") {
    // Identity step: every point should equal the initial condition.
    auto orbit = iterate_orbit(2.5, -1.0, 0, 10,
        [](double x, double y, double& nx, double& ny) { nx = x; ny = y; });
    CHECK(orbit.points.size() == 10);
    for (const auto& p : orbit.points) {
        CHECK(p.x == 2.5);
        CHECK(p.y == -1.0);
    }
}

TEST_CASE("Divergent step (× 10 each iter) trips the sentinel") {
    auto orbit = iterate_orbit(1.0, 1.0, 0, 100,
        [](double x, double y, double& nx, double& ny) { nx = x * 10.0; ny = y * 10.0; });
    CHECK(orbit.diverged);
    CHECK(orbit.points.size() < 100);
}
