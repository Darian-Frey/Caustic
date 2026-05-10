#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <caustic/colormap.hpp>
#include <caustic/generators/modular_chord.hpp>
#include <caustic/geometry_buffer.hpp>
#include <caustic/style.hpp>

#include "svg_renderer.hpp"

using namespace caustic;
namespace fs = std::filesystem;

namespace {

GeometryBuffer make_chord_buffer(int N = 30) {
    GeometryBuffer geo;
    geo.chords = modular_chord(N, 2.0);
    return geo;
}

GeometryBuffer make_polyline_buffer() {
    GeometryBuffer geo;
    std::vector<Vec2> p = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}};
    geo.polylines.push_back(p);
    return geo;
}

Style solid_style(Color c) {
    Style s;
    s.color_map = std::make_shared<Solid>(c);
    s.color_indexer = Indexer::ChordIndex;
    s.stroke = {0.8, 0.8, Indexer::ChordIndex, 0.7};
    s.background = {0.04, 0.04, 0.04, 1.0};
    return s;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

std::size_t count_occurrences(const std::string& haystack, const std::string& needle) {
    std::size_t n = 0;
    for (std::size_t pos = 0; (pos = haystack.find(needle, pos)) != std::string::npos; pos += needle.size()) ++n;
    return n;
}

}  // namespace

TEST_CASE("SVG output starts with XML preamble and svg root with viewBox") {
    const auto geo = make_chord_buffer();
    const auto style = solid_style({1, 0, 0, 1});
    const std::string svg = render_svg(geo, style);
    CHECK(svg.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 0) == 0);
    CHECK(contains(svg, "<svg xmlns=\"http://www.w3.org/2000/svg\""));
    CHECK(contains(svg, "viewBox=\"0 0 "));
    CHECK(contains(svg, "</svg>"));
}

TEST_CASE("SVG output is deterministic: same input → byte-identical output") {
    const auto geo = make_chord_buffer(50);
    const auto style = solid_style({0.3, 0.6, 0.9, 1.0});
    const std::string a = render_svg(geo, style);
    const std::string b = render_svg(geo, style);
    CHECK(a == b);
    CHECK(a.size() == b.size());
}

TEST_CASE("SVG chord count matches geometry") {
    const auto geo = make_chord_buffer(25);
    const auto style = solid_style({1, 1, 1, 1});
    const std::string svg = render_svg(geo, style);
    CHECK(count_occurrences(svg, "<line ") == 25);
}

TEST_CASE("SVG emits background <rect> in normal mode") {
    const auto geo = make_chord_buffer();
    const auto style = solid_style({1, 1, 1, 1});
    const std::string svg = render_svg(geo, style);
    CHECK(contains(svg, "<rect "));
    CHECK(contains(svg, "fill=\"#0a0a0a\""));
}

TEST_CASE("Plotter mode omits background, opacity, uses single colour") {
    const auto geo = make_chord_buffer(20);
    const auto style = solid_style({1, 0, 0, 1});
    SvgOptions opts;
    opts.plotter_mode = true;
    opts.plotter_color = "#000000";
    const std::string svg = render_svg(geo, style, opts);
    CHECK_FALSE(contains(svg, "<rect "));
    CHECK_FALSE(contains(svg, "stroke-opacity"));
    CHECK(contains(svg, "stroke=\"#000000\""));
    CHECK_FALSE(contains(svg, "stroke=\"#ff0000\""));
}

TEST_CASE("Plotter mode emits polylines as <polyline> for single-pen-down efficiency") {
    const auto geo = make_polyline_buffer();
    const auto style = solid_style({1, 1, 1, 1});
    SvgOptions opts;
    opts.plotter_mode = true;
    const std::string svg = render_svg(geo, style, opts);
    CHECK(contains(svg, "<polyline "));
    // No per-segment <line> elements for the polyline in plotter mode.
    CHECK(count_occurrences(svg, "<line ") == 0);
}

TEST_CASE("Coloured mode emits per-segment <line> for polylines") {
    const auto geo = make_polyline_buffer();  // 5 points → 4 segments
    auto style = solid_style({1, 0, 0, 1});
    style.color_map = std::make_shared<HsvSweep>(0.0, 360.0, 1.0, 1.0);
    const std::string svg = render_svg(geo, style);
    CHECK(count_occurrences(svg, "<line ") == 4);
    CHECK_FALSE(contains(svg, "<polyline "));
}

TEST_CASE("write_svg creates the file and round-trips through render_svg") {
    const fs::path tmp = fs::temp_directory_path() / "caustic_test_export.svg";
    const auto geo = make_chord_buffer(10);
    const auto style = solid_style({0.2, 0.5, 0.8, 1.0});
    write_svg(tmp, geo, style);
    REQUIRE(fs::exists(tmp));

    std::ifstream f(tmp);
    std::string from_disk((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(from_disk == render_svg(geo, style));

    fs::remove(tmp);
}

TEST_CASE("SVG coordinates use fixed 6-decimal precision") {
    const auto geo = make_chord_buffer(5);
    const auto style = solid_style({1, 1, 1, 1});
    const std::string svg = render_svg(geo, style);
    // Every coordinate should look like d+.dddddd (or have a sign in front).
    // Spot-check: find a coordinate token after x1=" and confirm it has 6 decimals.
    const auto pos = svg.find("x1=\"");
    REQUIRE(pos != std::string::npos);
    const auto start = pos + 4;
    const auto end = svg.find('"', start);
    REQUIRE(end != std::string::npos);
    const std::string coord = svg.substr(start, end - start);
    const auto dot = coord.find('.');
    REQUIRE(dot != std::string::npos);
    CHECK(coord.size() - dot - 1 == 6);
}

TEST_CASE("Empty geometry still produces a well-formed SVG (no chords / polylines)") {
    GeometryBuffer empty;
    const auto style = solid_style({1, 1, 1, 1});
    const std::string svg = render_svg(empty, style);
    CHECK(contains(svg, "<svg "));
    CHECK(contains(svg, "</svg>"));
    CHECK(count_occurrences(svg, "<line ") == 0);
}
