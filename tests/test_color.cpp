#include <doctest/doctest.h>

#include <cmath>

#include <caustic/color.hpp>
#include <caustic/colormap.hpp>

using namespace caustic;

namespace {
constexpr double kEps = 1e-9;

bool color_close(Color a, Color b, double eps = kEps) {
    return std::abs(a.r - b.r) < eps
        && std::abs(a.g - b.g) < eps
        && std::abs(a.b - b.b) < eps
        && std::abs(a.a - b.a) < eps;
}
}  // namespace

TEST_CASE("Color::lerp at endpoints returns endpoints") {
    Color a{1, 0, 0, 1};
    Color b{0, 1, 0, 1};
    CHECK(lerp(a, b, 0.0) == a);
    CHECK(lerp(a, b, 1.0) == b);
}

TEST_CASE("Color::lerp midpoint averages components") {
    Color a{1, 0, 0, 1};
    Color b{0, 1, 0, 0};
    Color m = lerp(a, b, 0.5);
    CHECK(color_close(m, {0.5, 0.5, 0.0, 0.5}));
}

TEST_CASE("Color::lerp clamps t outside [0, 1]") {
    Color a{1, 0, 0, 1};
    Color b{0, 1, 0, 1};
    CHECK(lerp(a, b, -0.5) == a);
    CHECK(lerp(a, b, 1.5) == b);
}

TEST_CASE("hsv_to_rgb known anchors") {
    CHECK(color_close(hsv_to_rgb(0.0,   1.0, 1.0), {1, 0, 0, 1}));
    CHECK(color_close(hsv_to_rgb(120.0, 1.0, 1.0), {0, 1, 0, 1}));
    CHECK(color_close(hsv_to_rgb(240.0, 1.0, 1.0), {0, 0, 1, 1}));
}

TEST_CASE("hsv_to_rgb wraps negative hue") {
    CHECK(color_close(hsv_to_rgb(-120.0, 1.0, 1.0), {0, 0, 1, 1}));
}

TEST_CASE("Solid colormap ignores t") {
    Solid s({0.3, 0.6, 0.9, 1.0});
    CHECK(s.at(0.0) == Color{0.3, 0.6, 0.9, 1.0});
    CHECK(s.at(0.5) == Color{0.3, 0.6, 0.9, 1.0});
    CHECK(s.at(1.0) == Color{0.3, 0.6, 0.9, 1.0});
}

TEST_CASE("LinearGradient interpolates between endpoints") {
    LinearGradient g({1, 0, 0, 1}, {0, 0, 1, 1});
    CHECK(g.at(0.0) == Color{1, 0, 0, 1});
    CHECK(g.at(1.0) == Color{0, 0, 1, 1});
    CHECK(color_close(g.at(0.5), {0.5, 0.0, 0.5, 1.0}));
}

TEST_CASE("HsvSweep at t=0 starts at hue_start") {
    HsvSweep h(0.0, 360.0, 1.0, 1.0);
    CHECK(color_close(h.at(0.0), {1, 0, 0, 1}));
}

TEST_CASE("Diverging colormap goes neg → mid → pos") {
    Diverging d({1, 0, 0, 1}, {1, 1, 1, 1}, {0, 0, 1, 1});
    CHECK(d.at(0.0) == Color{1, 0, 0, 1});
    CHECK(d.at(0.5) == Color{1, 1, 1, 1});
    CHECK(d.at(1.0) == Color{0, 0, 1, 1});
}
