#include <doctest/doctest.h>

#include <caustic/circle.hpp>
#include <caustic/sampler.hpp>

using caustic::Circle;
using caustic::sample_curve;

TEST_CASE("sample_curve returns N points for unit circle") {
    Circle c;
    const auto pts = sample_curve(c, 100);
    CHECK(pts.size() == 100);
}

TEST_CASE("sample_curve first point is at t=0, last at t=1") {
    Circle c;
    const auto pts = sample_curve(c, 5);
    REQUIRE(pts.size() == 5);
    CHECK(pts.front() == c.evaluate(0.0));
    CHECK(pts.back() == c.evaluate(1.0));
}

TEST_CASE("sample_curve returns empty when samples < 2") {
    Circle c;
    CHECK(sample_curve(c, 0).empty());
    CHECK(sample_curve(c, 1).empty());
}
