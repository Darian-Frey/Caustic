#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

#include <nlohmann/json.hpp>

#include <caustic/generators/diamond_stack.hpp>
#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>

using namespace caustic;

TEST_CASE("diamond_stack defaults to Both fans → 4*n_modules*N chords") {
    const auto cs = diamond_stack(3, 60, 0.6);
    CHECK(cs.size() == static_cast<std::size_t>(4 * 3 * 60));
}

TEST_CASE("diamond_stack Vertical-only → 2*n_modules*N chords") {
    const auto cs = diamond_stack(3, 60, 0.6, 0.0, /*fans=*/1);
    CHECK(cs.size() == static_cast<std::size_t>(2 * 3 * 60));
}

TEST_CASE("diamond_stack Horizontal-only → 2*n_modules*N chords") {
    const auto cs = diamond_stack(3, 60, 0.6, 0.0, /*fans=*/2);
    CHECK(cs.size() == static_cast<std::size_t>(2 * 3 * 60));
}

TEST_CASE("diamond_stack Vertical + Horizontal together == Both") {
    const auto vert  = diamond_stack(2, 30, 0.6, 0.0, 1);
    const auto horiz = diamond_stack(2, 30, 0.6, 0.0, 2);
    const auto both  = diamond_stack(2, 30, 0.6, 0.0, 0);
    CHECK(vert.size() + horiz.size() == both.size());
}

TEST_CASE("diamond_stack n_modules=1 with Both gives 4*N chords") {
    CHECK(diamond_stack(1, 40, 0.6).size() == 160);
}

TEST_CASE("diamond_stack with N<2 returns empty") {
    CHECK(diamond_stack(3, 1, 0.6).empty());
    CHECK(diamond_stack(3, 0, 0.6).empty());
}

TEST_CASE("diamond_stack with n_modules<=0 returns empty") {
    CHECK(diamond_stack(0, 60, 0.6).empty());
    CHECK(diamond_stack(-1, 60, 0.6).empty());
}

TEST_CASE("t_along is normalised to [0, 1)") {
    const auto cs = diamond_stack(3, 60, 0.6);
    for (const auto& c : cs) {
        CHECK(c.t_along >= 0.0);
        CHECK(c.t_along <= 1.0);
    }
}

TEST_CASE("t_along increases monotonically") {
    const auto cs = diamond_stack(3, 60, 0.6);
    for (std::size_t i = 1; i < cs.size(); ++i) {
        CHECK(cs[i].t_along >= cs[i - 1].t_along);
    }
}

TEST_CASE("rotation_rad=0 vertical-only first chord starts at top tip") {
    // Vertical-only mode emits upper fan first; chord 0 is (T, R).
    const auto cs = diamond_stack(1, 10, 0.6, 0.0, /*Vertical*/1);
    constexpr double eps = 1e-9;
    CHECK(std::abs(cs[0].a.x) < eps);   // top tip on axis
    CHECK(cs[0].b.x > 0.0);             // right waist
}

TEST_CASE("rotation_rad=pi/2 rotates stack to horizontal") {
    const auto vert  = diamond_stack(1, 10, 0.6, 0.0,                  /*Vertical*/1);
    const auto horiz = diamond_stack(1, 10, 0.6, std::numbers::pi / 2.0, /*Vertical*/1);
    // What was on the y-axis (vert[0].a.x ≈ 0) is now on the x-axis: horiz[0].a.y ≈ 0.
    constexpr double eps = 1e-6;
    CHECK(std::abs(horiz[0].a.y) < eps);
}

TEST_CASE("stack bounding box is approximately [-hw, -0.5] to [+hw, +0.5]") {
    const int nm = 3;
    const double asp = 0.6;
    const auto cs = diamond_stack(nm, 60, asp);
    const double mh = 1.0 / nm;
    const double hw = asp * mh * 0.5;
    for (const auto& c : cs) {
        for (const Vec2& v : {c.a, c.b}) {
            CHECK(v.x >= -hw - 1e-9);
            CHECK(v.x <= +hw + 1e-9);
            CHECK(v.y >= -0.5 - 1e-9);
            CHECK(v.y <= +0.5 + 1e-9);
        }
    }
}

TEST_CASE("preset round-trip: diamond_stack params survive JSON") {
    caustic::Preset p;
    auto& l = p.scene.layers[0];
    l.generator.type = caustic::GeneratorType::DiamondStack;
    l.generator.dstack.n_modules = 5;
    l.generator.dstack.N = 120;
    l.generator.dstack.aspect = 1.2;
    l.generator.dstack.rotation_rad = 0.785;

    nlohmann::json j;
    caustic::to_json(j, p);
    caustic::Preset p2;
    caustic::from_json(j, p2);

    const auto& d = p2.scene.layers[0].generator.dstack;
    CHECK(d.n_modules == 5);
    CHECK(d.N == 120);
    CHECK(std::abs(d.aspect - 1.2) < 1e-9);
    CHECK(std::abs(d.rotation_rad - 0.785) < 1e-9);
}
