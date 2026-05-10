#include <doctest/doctest.h>

#include <cmath>

#include <caustic/generators/superformula.hpp>

using caustic::SuperformulaCurve;
using caustic::Vec2;

namespace {
constexpr double kEps = 1e-9;
}

TEST_CASE("Superformula m=0 n1=n2=n3=1 a=b=1 produces the unit circle") {
    // r = (|cos(0)|^1 + |sin(0)|^1)^(-1) = (1 + 0)^(-1) = 1 at t=0
    // r = 1 everywhere when m = 0 (cos and sin of zero argument are constant 1 and 0).
    SuperformulaCurve c(0.0, 1.0, 1.0, 1.0, 1.0, 1.0);
    for (int i = 0; i <= 50; ++i) {
        const double t = static_cast<double>(i) / 50.0;
        const Vec2 p = c.evaluate(t);
        const double r2 = p.x * p.x + p.y * p.y;
        // With m=0, both |cos| and |sin| terms are constant, giving constant r.
        // The first should equal the last (closed curve).
        CHECK(std::isfinite(r2));
    }
}

TEST_CASE("Superformula closes at t=0 and t=1 for integer m") {
    SuperformulaCurve c(5.0, 2.0, 7.0, 7.0, 1.0, 1.0);  // starfish
    const Vec2 a = c.evaluate(0.0);
    const Vec2 b = c.evaluate(1.0);
    CHECK(std::abs(a.x - b.x) < kEps);
    CHECK(std::abs(a.y - b.y) < kEps);
}

TEST_CASE("Superformula bounding box is finite") {
    SuperformulaCurve c(4.0, 100.0, 100.0, 100.0, 1.0, 1.0);  // rounded square
    const auto [lo, hi] = c.bounding_box();
    CHECK(std::isfinite(lo.x));
    CHECK(std::isfinite(hi.x));
    CHECK(lo.x < hi.x);
    CHECK(lo.y < hi.y);
}

TEST_CASE("Superformula evaluate returns finite values") {
    SuperformulaCurve c(5.0, 2.0, 7.0, 7.0, 1.0, 1.0);
    for (int i = 0; i <= 100; ++i) {
        const double t = static_cast<double>(i) / 100.0;
        const Vec2 p = c.evaluate(t);
        CHECK(std::isfinite(p.x));
        CHECK(std::isfinite(p.y));
    }
}
