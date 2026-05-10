#include <doctest/doctest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>

using namespace caustic;
using nlohmann::json;
namespace fs = std::filesystem;

namespace {

bool color_close(Color a, Color b, double eps = 0.005) {
    return std::abs(a.r - b.r) < eps
        && std::abs(a.g - b.g) < eps
        && std::abs(a.b - b.b) < eps;
}

Preset make_modular_preset() {
    Preset p;
    p.name = "test_modular";
    p.scene.layers[0].generator.type = GeneratorType::ModularChord;
    p.scene.layers[0].generator.chord = {200, 2.5};
    p.scene.layers[0].style.colormap_type = ColorMapType::HsvSweep;
    p.scene.layers[0].style.hue_start = 10.0;
    p.scene.layers[0].style.hue_end = 350.0;
    p.scene.layers[0].style.hsv_saturation = 0.7;
    p.scene.layers[0].style.hsv_value = 0.9;
    p.scene.layers[0].style.color_indexer = Indexer::ChordLength;
    p.scene.layers[0].style.stroke_width_min = 0.5;
    p.scene.layers[0].style.stroke_width_max = 1.5;
    p.scene.layers[0].style.stroke_width_indexer = Indexer::Angle;
    p.scene.layers[0].style.opacity = 0.7;
    p.scene.background = {0.05, 0.05, 0.10, 1.0};
    p.scene.layers[0].style.cyclic = false;
    p.camera = {12.5, -7.0, 1.4};
    return p;
}

}  // namespace

TEST_CASE("color hex round-trips for canonical anchors") {
    using detail::color_to_hex;
    using detail::color_from_hex;
    CHECK(color_to_hex({0.0, 0.0, 0.0, 1.0}) == "#000000");
    CHECK(color_to_hex({1.0, 1.0, 1.0, 1.0}) == "#ffffff");
    CHECK(color_to_hex({1.0, 0.0, 0.0, 1.0}) == "#ff0000");
    CHECK(color_close(color_from_hex("#1a4480"), {0x1a / 255.0, 0x44 / 255.0, 0x80 / 255.0, 1.0}));
}

TEST_CASE("indexer string round-trip covers all four variants") {
    using detail::indexer_to_string;
    using detail::indexer_from_string;
    for (Indexer i : {Indexer::ChordIndex, Indexer::ChordLength, Indexer::Angle, Indexer::CurveT}) {
        CHECK(indexer_from_string(indexer_to_string(i)) == i);
    }
}

TEST_CASE("generator type string round-trip covers all four") {
    using detail::generator_type_to_string;
    using detail::generator_type_from_string;
    for (auto t : {GeneratorType::ModularChord, GeneratorType::Hypotrochoid,
                   GeneratorType::Epitrochoid, GeneratorType::Lissajous}) {
        CHECK(generator_type_from_string(generator_type_to_string(t)) == t);
    }
}

TEST_CASE("colormap type string round-trip covers all four") {
    using detail::colormap_type_to_string;
    using detail::colormap_type_from_string;
    for (auto t : {ColorMapType::Solid, ColorMapType::LinearGradient,
                   ColorMapType::HsvSweep, ColorMapType::Diverging}) {
        CHECK(colormap_type_from_string(colormap_type_to_string(t)) == t);
    }
}

TEST_CASE("Preset round-trips through JSON for modular_chord + HsvSweep") {
    const Preset original = make_modular_preset();
    json j;
    to_json(j, original);
    Preset back;
    from_json(j, back);

    CHECK(back.version == original.version);
    CHECK(back.name == original.name);
    CHECK(back.scene.layers[0].generator.type == GeneratorType::ModularChord);
    CHECK(back.scene.layers[0].generator.chord.N == original.scene.layers[0].generator.chord.N);
    CHECK(back.scene.layers[0].generator.chord.k == original.scene.layers[0].generator.chord.k);
    CHECK(back.scene.layers[0].style.colormap_type == ColorMapType::HsvSweep);
    CHECK(back.scene.layers[0].style.hue_start == original.scene.layers[0].style.hue_start);
    CHECK(back.scene.layers[0].style.hue_end == original.scene.layers[0].style.hue_end);
    CHECK(back.scene.layers[0].style.color_indexer == Indexer::ChordLength);
    CHECK(back.scene.layers[0].style.stroke_width_min == original.scene.layers[0].style.stroke_width_min);
    CHECK(back.scene.layers[0].style.stroke_width_max == original.scene.layers[0].style.stroke_width_max);
    CHECK(back.scene.layers[0].style.stroke_width_indexer == Indexer::Angle);
    CHECK(back.scene.layers[0].style.opacity == original.scene.layers[0].style.opacity);
    CHECK(color_close(back.scene.background, original.scene.background));
    CHECK(back.camera.pan_x_px == original.camera.pan_x_px);
    CHECK(back.camera.pan_y_px == original.camera.pan_y_px);
    CHECK(back.camera.zoom == original.camera.zoom);
}

TEST_CASE("Preset round-trips for hypotrochoid + LinearGradient") {
    Preset p;
    p.name = "spiro";
    p.scene.layers[0].generator.type = GeneratorType::Hypotrochoid;
    p.scene.layers[0].generator.hypo = {5.0, 3.0, 2.0, 4000};
    p.scene.layers[0].style.colormap_type = ColorMapType::LinearGradient;
    p.scene.layers[0].style.gradient_start = {0.10, 0.27, 0.50, 1.0};
    p.scene.layers[0].style.gradient_end   = {0.94, 0.75, 0.31, 1.0};
    p.scene.layers[0].style.cyclic = true;

    json j;
    to_json(j, p);
    Preset back;
    from_json(j, back);

    CHECK(back.scene.layers[0].generator.type == GeneratorType::Hypotrochoid);
    CHECK(back.scene.layers[0].generator.hypo.R == 5.0);
    CHECK(back.scene.layers[0].generator.hypo.r == 3.0);
    CHECK(back.scene.layers[0].generator.hypo.d == 2.0);
    CHECK(back.scene.layers[0].generator.hypo.samples == 4000);
    CHECK(back.scene.layers[0].style.colormap_type == ColorMapType::LinearGradient);
    CHECK(color_close(back.scene.layers[0].style.gradient_start, p.scene.layers[0].style.gradient_start));
    CHECK(color_close(back.scene.layers[0].style.gradient_end,   p.scene.layers[0].style.gradient_end));
    CHECK(back.scene.layers[0].style.cyclic == true);
}

TEST_CASE("Preset round-trips for Lissajous + Diverging") {
    Preset p;
    p.name = "bowtie";
    p.scene.layers[0].generator.type = GeneratorType::Lissajous;
    p.scene.layers[0].generator.liss = {1.0, 1.0, 1.0, 2.0, 1.5707963267948966, 4000};
    p.scene.layers[0].style.colormap_type = ColorMapType::Diverging;
    p.scene.layers[0].style.div_negative = {0.95, 0.55, 0.20, 1.0};
    p.scene.layers[0].style.div_midpoint = {0.95, 0.95, 0.95, 1.0};
    p.scene.layers[0].style.div_positive = {0.20, 0.70, 0.85, 1.0};

    json j;
    to_json(j, p);
    Preset back;
    from_json(j, back);

    CHECK(back.scene.layers[0].generator.liss.a == 1.0);
    CHECK(back.scene.layers[0].generator.liss.b == 2.0);
    CHECK(back.scene.layers[0].style.colormap_type == ColorMapType::Diverging);
    CHECK(color_close(back.scene.layers[0].style.div_negative, p.scene.layers[0].style.div_negative));
    CHECK(color_close(back.scene.layers[0].style.div_midpoint, p.scene.layers[0].style.div_midpoint));
    CHECK(color_close(back.scene.layers[0].style.div_positive, p.scene.layers[0].style.div_positive));
}

TEST_CASE("Preset round-trips for Solid colormap") {
    Preset p;
    p.scene.layers[0].generator.type = GeneratorType::Epitrochoid;
    p.scene.layers[0].generator.epi = {3.0, 1.0, 1.5, 4000};
    p.scene.layers[0].style.colormap_type = ColorMapType::Solid;
    p.scene.layers[0].style.solid_color = {0.5, 0.7, 0.3, 1.0};

    json j;
    to_json(j, p);
    Preset back;
    from_json(j, back);
    CHECK(back.scene.layers[0].style.colormap_type == ColorMapType::Solid);
    CHECK(color_close(back.scene.layers[0].style.solid_color, p.scene.layers[0].style.solid_color));
}

TEST_CASE("from_json rejects unknown version") {
    json j = R"({
        "version": 99,
        "name": "future",
        "generator": {"type": "modular_chord", "params": {"N": 200, "k": 2.0}},
        "style": {
            "color_map": {"type": "solid", "color": "#ffffff"},
            "color_indexer": "by_chord_index",
            "stroke": {"width_min": 1.0, "width_max": 1.0, "width_indexer": "by_chord_index", "opacity": 1.0},
            "background": "#000000",
            "cyclic": false
        },
        "camera": {"pan_x_px": 0.0, "pan_y_px": 0.0, "zoom": 1.0}
    })"_json;
    Preset p;
    CHECK_THROWS(from_json(j, p));
}

TEST_CASE("save_preset and load_preset round-trip on disk") {
    const fs::path tmp = fs::temp_directory_path() / "caustic_test_preset.json";
    const Preset original = make_modular_preset();
    save_preset(tmp, original);

    REQUIRE(fs::exists(tmp));
    const Preset back = load_preset(tmp);
    CHECK(back.name == original.name);
    CHECK(back.scene.layers[0].generator.chord.N == original.scene.layers[0].generator.chord.N);
    CHECK(back.scene.layers[0].generator.chord.k == original.scene.layers[0].generator.chord.k);

    fs::remove(tmp);
}

TEST_CASE("user_preset_dir respects XDG_CONFIG_HOME") {
    // Save the env var, set our own, restore.
    const char* prev = std::getenv("XDG_CONFIG_HOME");
    setenv("XDG_CONFIG_HOME", "/tmp/caustic-xdg-test", 1);
    CHECK(user_preset_dir() == fs::path("/tmp/caustic-xdg-test/caustic/presets"));
    if (prev) setenv("XDG_CONFIG_HOME", prev, 1);
    else      unsetenv("XDG_CONFIG_HOME");
}

TEST_CASE("bundled preset files parse cleanly") {
    const fs::path repo_presets = "presets";
    if (!fs::exists(repo_presets)) {
        // Tests may run from a build subdir; try ../presets too.
        const fs::path alt = fs::path("..") / "presets";
        if (!fs::exists(alt)) {
            MESSAGE("skipping bundled-preset parse test — presets/ not found from cwd");
            return;
        }
    }
    const fs::path dir = fs::exists(repo_presets) ? repo_presets : fs::path("..") / "presets";
    int parsed = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.path().extension() != ".json") continue;
        Preset p = load_preset(entry.path());
        // After auto-promote, every loaded preset is v2 in memory regardless
        // of the on-disk version. The bundled set is still on-disk v1 today
        // and will be migrated to v2 in a follow-up commit.
        CHECK(p.version == 2);
        CHECK_FALSE(p.name.empty());
        CHECK_FALSE(p.scene.layers.empty());
        parsed++;
    }
    CHECK(parsed >= 14);  // 5 v1 + 3 Stage B (rose, super, phyl) + 6 Phase 10 (corner, bowtie, deltoid, hexagram, four-bowties, rgb)
}
