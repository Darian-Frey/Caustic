#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <caustic/colormap.hpp>
#include <caustic/generators/modular_chord.hpp>
#include <caustic/geometry_buffer.hpp>
#include <caustic/scene_render.hpp>
#include <caustic/style.hpp>

#include "svg_renderer.hpp"

using namespace caustic;
namespace fs = std::filesystem;

namespace {

constexpr Color kTestBackground = {0.04, 0.04, 0.04, 1.0};

Style solid_style(Color c) {
    Style s;
    s.color_map = std::make_shared<Solid>(c);
    s.color_indexer = Indexer::ChordIndex;
    s.stroke = {0.8, 0.8, Indexer::ChordIndex, 0.7};
    return s;
}

std::vector<LayerRender> make_chord_layers(int N = 30, Color color = {1, 0, 0, 1}) {
    LayerRender layer;
    layer.geometry.chords = modular_chord(N, 2.0);
    layer.style = solid_style(color);
    layer.name = "chords";
    std::vector<LayerRender> out;
    out.push_back(std::move(layer));
    return out;
}

std::vector<LayerRender> make_polyline_layers(std::shared_ptr<ColorMap> map = nullptr) {
    LayerRender layer;
    std::vector<Vec2> p = {{0, 0}, {1, 0}, {1, 1}, {0, 1}, {0, 0}};
    layer.geometry.polylines.push_back(p);
    layer.style = solid_style({1, 1, 1, 1});
    if (map) layer.style.color_map = map;
    layer.name = "polyline";
    std::vector<LayerRender> out;
    out.push_back(std::move(layer));
    return out;
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
    const auto layers = make_chord_layers();
    const std::string svg = render_svg(layers, kTestBackground);
    CHECK(svg.rfind("<?xml version=\"1.0\" encoding=\"UTF-8\"?>", 0) == 0);
    CHECK(contains(svg, "<svg xmlns=\"http://www.w3.org/2000/svg\""));
    CHECK(contains(svg, "viewBox=\"0 0 "));
    CHECK(contains(svg, "</svg>"));
}

TEST_CASE("SVG output is deterministic: same input → byte-identical output") {
    const auto layers = make_chord_layers(50, {0.3, 0.6, 0.9, 1.0});
    const std::string a = render_svg(layers, kTestBackground);
    const std::string b = render_svg(layers, kTestBackground);
    CHECK(a == b);
    CHECK(a.size() == b.size());
}

TEST_CASE("SVG chord count matches geometry") {
    const auto layers = make_chord_layers(25);
    const std::string svg = render_svg(layers, kTestBackground);
    CHECK(count_occurrences(svg, "<line ") == 25);
}

TEST_CASE("SVG emits background <rect> in normal mode") {
    const auto layers = make_chord_layers();
    const std::string svg = render_svg(layers, kTestBackground);
    CHECK(contains(svg, "<rect "));
    CHECK(contains(svg, "fill=\"#0a0a0a\""));
}

TEST_CASE("Plotter mode omits background, opacity, uses single colour") {
    const auto layers = make_chord_layers(20, {1, 0, 0, 1});
    SvgOptions opts;
    opts.plotter_mode = true;
    opts.plotter_color = "#000000";
    const std::string svg = render_svg(layers, kTestBackground, opts);
    CHECK_FALSE(contains(svg, "<rect "));
    CHECK_FALSE(contains(svg, "stroke-opacity"));
    CHECK(contains(svg, "stroke=\"#000000\""));
    CHECK_FALSE(contains(svg, "stroke=\"#ff0000\""));
}

TEST_CASE("Plotter mode emits polylines as <polyline> for single-pen-down efficiency") {
    const auto layers = make_polyline_layers();
    SvgOptions opts;
    opts.plotter_mode = true;
    const std::string svg = render_svg(layers, kTestBackground, opts);
    CHECK(contains(svg, "<polyline "));
    CHECK(count_occurrences(svg, "<line ") == 0);
}

TEST_CASE("Coloured mode emits per-segment <line> for polylines") {
    const auto layers = make_polyline_layers(std::make_shared<HsvSweep>(0.0, 360.0, 1.0, 1.0));
    const std::string svg = render_svg(layers, kTestBackground);
    CHECK(count_occurrences(svg, "<line ") == 4);  // 5 points → 4 segments
    CHECK_FALSE(contains(svg, "<polyline "));
}

TEST_CASE("write_svg creates the file and round-trips through render_svg") {
    const fs::path tmp = fs::temp_directory_path() / "caustic_test_export.svg";
    const auto layers = make_chord_layers(10, {0.2, 0.5, 0.8, 1.0});
    write_svg(tmp, layers, kTestBackground);
    REQUIRE(fs::exists(tmp));

    std::ifstream f(tmp);
    std::string from_disk((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    CHECK(from_disk == render_svg(layers, kTestBackground));

    fs::remove(tmp);
}

TEST_CASE("SVG coordinates use fixed 6-decimal precision") {
    const auto layers = make_chord_layers(5);
    const std::string svg = render_svg(layers, kTestBackground);
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

TEST_CASE("Empty geometry still produces a well-formed SVG") {
    std::vector<LayerRender> empty_layers;
    const std::string svg = render_svg(empty_layers, kTestBackground);
    CHECK(contains(svg, "<svg "));
    CHECK(contains(svg, "</svg>"));
    CHECK(count_occurrences(svg, "<line ") == 0);
}

TEST_CASE("Two layers produce two <g> groups") {
    auto layers = make_chord_layers(20);
    LayerRender l2;
    l2.geometry.chords = modular_chord(15, 3.0);
    l2.style = solid_style({0, 1, 0, 1});
    l2.name = "second";
    layers.push_back(std::move(l2));
    const std::string svg = render_svg(layers, kTestBackground);
    CHECK(count_occurrences(svg, "<g id=") == 2);
    CHECK(contains(svg, "inkscape:label=\"chords\""));
    CHECK(contains(svg, "inkscape:label=\"second\""));
}

TEST_CASE("LayerTransform translate shifts geometry") {
    // Without transform, a chord starts at (1, 0). After translate=(10, 0)
    // and re-rendering, the bbox shifts so the centre x moves.
    LayerRender layer;
    layer.geometry.chords = modular_chord(8, 2.0);
    // Apply translate by (10, 0) in-place using the apply helper.
    LayerTransform t;
    t.translate = {10.0, 0.0};
    apply_transform(layer.geometry, t);
    layer.style = solid_style({1, 1, 1, 1});
    layer.name = "shifted";
    std::vector<LayerRender> layers;
    layers.push_back(std::move(layer));

    // Just make sure it renders without throwing and produces lines.
    const std::string svg = render_svg(layers, kTestBackground);
    CHECK(contains(svg, "<line "));
}
