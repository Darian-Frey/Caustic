#include <doctest/doctest.h>

#include <cmath>

#include <nlohmann/json.hpp>

#include <caustic/generators/custom_chord.hpp>
#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>

using namespace caustic;

TEST_CASE("custom_chord emits one chord per valid pair") {
    std::vector<Vec2> nails = {{0, 0}, {1, 0}, {0, 1}, {1, 1}};
    std::vector<std::pair<int, int>> pairs = {{0, 1}, {1, 2}, {2, 3}};
    const auto cs = custom_chord(nails, pairs);
    CHECK(cs.size() == 3);
    CHECK(cs[0].a.x == 0); CHECK(cs[0].a.y == 0);
    CHECK(cs[0].b.x == 1); CHECK(cs[0].b.y == 0);
    CHECK(cs[2].a.x == 0); CHECK(cs[2].a.y == 1);
    CHECK(cs[2].b.x == 1); CHECK(cs[2].b.y == 1);
}

TEST_CASE("custom_chord skips out-of-range indices") {
    std::vector<Vec2> nails = {{0, 0}, {1, 0}};
    std::vector<std::pair<int, int>> pairs = {{0, 1}, {5, 0}, {-1, 1}};
    const auto cs = custom_chord(nails, pairs);
    CHECK(cs.size() == 1);
}

TEST_CASE("custom_chord with empty inputs returns empty") {
    CHECK(custom_chord({}, {}).empty());
    CHECK(custom_chord({{0, 0}}, {}).empty());
    CHECK(custom_chord({}, {{0, 1}}).empty());
}

TEST_CASE("custom_chord t_along sweeps 0..1 monotonically") {
    std::vector<Vec2> nails = {{0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0}};
    std::vector<std::pair<int, int>> pairs = {{0, 1}, {1, 2}, {2, 3}, {3, 4}};
    const auto cs = custom_chord(nails, pairs);
    REQUIRE(cs.size() == 4);
    for (std::size_t i = 1; i < cs.size(); ++i) {
        CHECK(cs[i].t_along > cs[i - 1].t_along);
    }
    CHECK(cs[0].t_along == 0.0);
    CHECK(cs.back().t_along < 1.0);
}

TEST_CASE("preset round-trip: custom_chord params survive JSON") {
    caustic::Preset p;
    auto& l = p.scene.layers[0];
    l.generator.type = caustic::GeneratorType::CustomChord;
    l.generator.custom.nails = {{0.0, 1.0}, {0.7, 0.7}, {1.0, 0.0}, {0.7, -0.7}};
    l.generator.custom.chords = {{0, 2}, {1, 3}};

    nlohmann::json j;
    caustic::to_json(j, p);
    caustic::Preset p2;
    caustic::from_json(j, p2);

    const auto& c = p2.scene.layers[0].generator.custom;
    REQUIRE(c.nails.size() == 4);
    REQUIRE(c.chords.size() == 2);
    CHECK(std::abs(c.nails[0].x - 0.0) < 1e-9);
    CHECK(std::abs(c.nails[0].y - 1.0) < 1e-9);
    CHECK(std::abs(c.nails[1].x - 0.7) < 1e-9);
    CHECK(std::abs(c.nails[3].y - (-0.7)) < 1e-9);
    CHECK(c.chords[0].first  == 0);
    CHECK(c.chords[0].second == 2);
    CHECK(c.chords[1].first  == 1);
    CHECK(c.chords[1].second == 3);
}

TEST_CASE("preset round-trip: per-chord widths and opacities survive JSON") {
    caustic::Preset p;
    auto& l = p.scene.layers[0];
    l.generator.type = caustic::GeneratorType::CustomChord;
    l.generator.custom.nails = {{0, 0}, {1, 0}, {0, 1}};
    l.generator.custom.chords = {{0, 1}, {1, 2}};
    l.generator.custom.chord_widths    = {0.5, 2.5};
    l.generator.custom.chord_opacities = {0.2, 0.9};

    nlohmann::json j;
    caustic::to_json(j, p);
    caustic::Preset p2;
    caustic::from_json(j, p2);

    const auto& c = p2.scene.layers[0].generator.custom;
    REQUIRE(c.chord_widths.size() == 2);
    REQUIRE(c.chord_opacities.size() == 2);
    CHECK(std::abs(c.chord_widths[0]    - 0.5) < 1e-9);
    CHECK(std::abs(c.chord_widths[1]    - 2.5) < 1e-9);
    CHECK(std::abs(c.chord_opacities[0] - 0.2) < 1e-9);
    CHECK(std::abs(c.chord_opacities[1] - 0.9) < 1e-9);
}
