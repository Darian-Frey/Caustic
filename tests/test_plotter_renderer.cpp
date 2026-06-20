#include <doctest/doctest.h>

#include <filesystem>
#include <string>

#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>
#include <caustic/scene_render.hpp>

#include "plotter_renderer.hpp"

namespace fs = std::filesystem;
using namespace caustic;

namespace {

// Build a tiny scene with a single chord — used by the format-header / fmt
// tests. Avoids the cost of building a real preset and keeps assertions
// focused on the emitter, not the generator pipeline.
std::vector<LayerRender> one_chord_scene(Vec2 a = {-0.5, -0.5},
                                         Vec2 b = { 0.5,  0.5}) {
    LayerRender L;
    L.geometry.chords.push_back(Chord{a, b});
    L.style = Style{};
    L.name = "test";
    return {std::move(L)};
}

}  // namespace

// ---------------------------------------------------------------------------
// G-code

TEST_CASE("render_gcode emits a Grbl-compatible header and footer") {
    const auto layers = one_chord_scene();
    const std::string out = render_gcode(layers);
    // Header
    CHECK(out.find("G21") != std::string::npos);    // mm
    CHECK(out.find("G90") != std::string::npos);    // absolute coords
    CHECK(out.find("G94") != std::string::npos);    // unit/min feed
    // Pen up before the first travel
    CHECK(out.find("; pen up") != std::string::npos);
    // Footer
    CHECK(out.find("M2") != std::string::npos);     // program end
    CHECK(out.find("return home") != std::string::npos);
}

TEST_CASE("render_gcode lowers the pen, draws, and raises it for each path") {
    const auto layers = one_chord_scene();
    const std::string out = render_gcode(layers);
    // One chord → one pen-down + one pen-up pair (plus the initial header).
    const std::size_t down_count =
        [&]{ std::size_t n = 0, pos = 0;
             while ((pos = out.find("; pen down", pos)) != std::string::npos) { ++n; ++pos; }
             return n; }();
    const std::size_t up_count =
        [&]{ std::size_t n = 0, pos = 0;
             while ((pos = out.find("; pen up", pos)) != std::string::npos) { ++n; ++pos; }
             return n; }();
    CHECK(down_count == 1);
    CHECK(up_count   == 2);  // header pen-up + post-draw pen-up
}

TEST_CASE("render_gcode is deterministic — same input → same bytes") {
    const auto layers = one_chord_scene();
    const std::string a = render_gcode(layers);
    const std::string b = render_gcode(layers);
    CHECK(a == b);
}

TEST_CASE("render_gcode coordinates land inside the page with margin") {
    const auto layers = one_chord_scene({-1, -1}, {1, 1});
    PlotterOptions opts;
    opts.width_mm = opts.height_mm = 100.0;
    opts.margin = 0.1;
    const std::string out = render_gcode(layers, opts);
    // Bounds — the diagonal from (-1,-1) to (1,1) should be centred on
    // (50, 50) mm and inset by 10 mm margin, i.e. land somewhere in
    // (10, 10) … (90, 90). Spot-check that no coordinate exceeds the
    // page extent or goes negative.
    const std::string::size_type first_y = out.find("G0 X");
    REQUIRE(first_y != std::string::npos);
    const double x = std::stod(out.substr(first_y + 4));
    CHECK(x >= 10.0);
    CHECK(x <= 90.0);
}

TEST_CASE("render_gcode honours custom feedrates + Z heights") {
    const auto layers = one_chord_scene();
    PlotterOptions opts;
    opts.pen_up_z        = 3.5;
    opts.pen_down_z      = -0.2;
    opts.travel_feedrate = 7777.0;
    opts.draw_feedrate   = 1234.0;
    const std::string out = render_gcode(layers, opts);
    CHECK(out.find("Z3.500")    != std::string::npos);
    CHECK(out.find("Z-0.200")   != std::string::npos);
    CHECK(out.find("F7777.000") != std::string::npos);
    CHECK(out.find("F1234.000") != std::string::npos);
}

TEST_CASE("render_gcode on an empty scene still emits valid header + footer") {
    const std::vector<LayerRender> empty;
    const std::string out = render_gcode(empty);
    CHECK(out.find("G21") != std::string::npos);
    CHECK(out.find("M2")  != std::string::npos);
}

// ---------------------------------------------------------------------------
// HPGL

TEST_CASE("render_hpgl emits IN; / SP; header and footer") {
    const auto layers = one_chord_scene();
    const std::string out = render_hpgl(layers);
    CHECK(out.starts_with("IN;\n"));
    CHECK(out.find("SP1;") != std::string::npos);
    CHECK(out.find("SP0;") != std::string::npos);
    // Pen up + pen down at least once each.
    CHECK(out.find("PU") != std::string::npos);
    CHECK(out.find("PD") != std::string::npos);
}

TEST_CASE("render_hpgl honours pen_number") {
    const auto layers = one_chord_scene();
    PlotterOptions opts;
    opts.pen_number = 3;
    const std::string out = render_hpgl(layers, opts);
    CHECK(out.find("SP3;") != std::string::npos);
}

TEST_CASE("render_hpgl coordinates are integer plotter units (no decimals)") {
    const auto layers = one_chord_scene();
    const std::string out = render_hpgl(layers);
    // No '.' should appear in the coordinate stream — HPGL takes integers
    // in plotter units. The comment-like ';' is the only punctuation.
    CHECK(out.find('.') == std::string::npos);
}

TEST_CASE("render_hpgl is deterministic") {
    const auto layers = one_chord_scene();
    const std::string a = render_hpgl(layers);
    const std::string b = render_hpgl(layers);
    CHECK(a == b);
}

// ---------------------------------------------------------------------------
// Disk I/O

TEST_CASE("write_gcode + write_hpgl create the file and parent directories") {
    const fs::path tmp_dir = fs::temp_directory_path() / "caustic_plotter_test";
    fs::remove_all(tmp_dir);
    const fs::path gcode_path = tmp_dir / "nested" / "out.gcode";
    const fs::path hpgl_path  = tmp_dir / "nested" / "out.hpgl";

    const auto layers = one_chord_scene();
    write_gcode(gcode_path, layers);
    write_hpgl(hpgl_path,   layers);

    CHECK(fs::exists(gcode_path));
    CHECK(fs::exists(hpgl_path));
    CHECK(fs::file_size(gcode_path) > 0);
    CHECK(fs::file_size(hpgl_path)  > 0);

    fs::remove_all(tmp_dir);
}

// ---------------------------------------------------------------------------
// Round-trip from bundled preset

TEST_CASE("bundled preset → gcode → text contains expected commands") {
    const fs::path repo_presets = "presets";
    const fs::path alt          = fs::path("..") / "presets";
    const fs::path dir = fs::exists(repo_presets) ? repo_presets : alt;
    if (!fs::exists(dir)) {
        MESSAGE("skipping bundled-preset gcode test — presets/ not found");
        return;
    }
    const fs::path one = dir / "cardioid_classic.json";
    if (!fs::exists(one)) {
        MESSAGE("skipping — cardioid_classic preset missing");
        return;
    }
    const Preset p = load_preset(one);
    const auto layers = build_renderables(p.scene);
    const std::string out = render_gcode(layers);
    // The cardioid has 200 chords by default — we should see *many*
    // G0/G1 motion commands.
    std::size_t g0 = 0, g1 = 0, pos = 0;
    while ((pos = out.find("G0 ", pos)) != std::string::npos) { ++g0; ++pos; }
    pos = 0;
    while ((pos = out.find("G1 ", pos)) != std::string::npos) { ++g1; ++pos; }
    CHECK(g0 > 50);
    CHECK(g1 > 50);
}
