#include <doctest/doctest.h>

#include <cmath>

#include <caustic/array_tools.hpp>
#include <caustic/preset.hpp>

using namespace caustic;

namespace {

Layer make_unit_layer() {
    Layer l;
    l.name = "src";
    l.transform.translate = {1.0, 0.0};
    l.transform.rotate_rad = 0.0;
    return l;
}

}  // namespace

TEST_CASE("rotational_array returns N layers") {
    const auto layers = rotational_array(make_unit_layer(), 6);
    CHECK(layers.size() == 6);
}

TEST_CASE("rotational_array with N=0 returns empty") {
    const auto layers = rotational_array(make_unit_layer(), 0);
    CHECK(layers.empty());
}

TEST_CASE("rotational_array places copies around origin") {
    // Source at (1, 0). With N=4 around origin, copies should be at
    // (1,0), (0,1), (-1,0), (0,-1).
    const auto layers = rotational_array(make_unit_layer(), 4);
    REQUIRE(layers.size() == 4);
    constexpr double eps = 1e-9;
    CHECK(std::abs(layers[0].transform.translate.x - 1.0) < eps);
    CHECK(std::abs(layers[0].transform.translate.y - 0.0) < eps);
    CHECK(std::abs(layers[1].transform.translate.x - 0.0) < eps);
    CHECK(std::abs(layers[1].transform.translate.y - 1.0) < eps);
    CHECK(std::abs(layers[2].transform.translate.x + 1.0) < eps);
    CHECK(std::abs(layers[2].transform.translate.y - 0.0) < eps);
    CHECK(std::abs(layers[3].transform.translate.x - 0.0) < eps);
    CHECK(std::abs(layers[3].transform.translate.y + 1.0) < eps);
}

TEST_CASE("rotational_array adds rotation angles") {
    const auto layers = rotational_array(make_unit_layer(), 4);
    REQUIRE(layers.size() == 4);
    constexpr double pi = 3.14159265358979323846;
    constexpr double eps = 1e-9;
    CHECK(std::abs(layers[0].transform.rotate_rad - 0.0) < eps);
    CHECK(std::abs(layers[1].transform.rotate_rad - pi / 2.0) < eps);
    CHECK(std::abs(layers[2].transform.rotate_rad - pi) < eps);
    CHECK(std::abs(layers[3].transform.rotate_rad - 3.0 * pi / 2.0) < eps);
}

TEST_CASE("grid_tile produces rows*cols layers") {
    Layer src;
    src.transform.translate = {0.0, 0.0};
    const auto layers = grid_tile(src, 2, 4, {1.0, 1.0});
    CHECK(layers.size() == 8);
}

TEST_CASE("grid_tile centres copies around source translate") {
    Layer src;
    src.transform.translate = {0.0, 0.0};
    // 3x3 grid with spacing 1.0 — corners should be at ±1 from centre.
    const auto layers = grid_tile(src, 3, 3, {1.0, 1.0});
    REQUIRE(layers.size() == 9);
    constexpr double eps = 1e-9;
    // Top-left corner = layer[0,0]
    CHECK(std::abs(layers[0].transform.translate.x + 1.0) < eps);
    CHECK(std::abs(layers[0].transform.translate.y + 1.0) < eps);
    // Centre = layer[1,1] (index 4 in row-major)
    CHECK(std::abs(layers[4].transform.translate.x) < eps);
    CHECK(std::abs(layers[4].transform.translate.y) < eps);
    // Bottom-right = layer[2,2] (index 8)
    CHECK(std::abs(layers[8].transform.translate.x - 1.0) < eps);
    CHECK(std::abs(layers[8].transform.translate.y - 1.0) < eps);
}

TEST_CASE("mirror_reflect Y axis flips x") {
    Layer src;
    src.transform.translate = {1.5, 0.5};
    const auto layers = mirror_reflect(src, MirrorAxis::Y);
    REQUIRE(layers.size() == 2);
    CHECK(layers[0].transform.translate.x == 1.5);
    CHECK(layers[1].transform.translate.x == -1.5);
    CHECK(layers[1].transform.mirror_x);
    CHECK_FALSE(layers[1].transform.mirror_y);
}

TEST_CASE("mirror_reflect X axis flips y") {
    Layer src;
    src.transform.translate = {1.5, 0.5};
    const auto layers = mirror_reflect(src, MirrorAxis::X);
    REQUIRE(layers.size() == 2);
    CHECK(layers[1].transform.translate.y == -0.5);
    CHECK(layers[1].transform.mirror_y);
    CHECK_FALSE(layers[1].transform.mirror_x);
}
