#pragma once

#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include <caustic/camera.hpp>
#include <caustic/color.hpp>
#include <caustic/indexer.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// ---------------------------------------------------------------------------
// Generator specs

enum class GeneratorType {
    ModularChord,
    Hypotrochoid,
    Epitrochoid,
    Lissajous,
    Rose,
    Superformula,
    Phyllotaxis,
    PolygonChord,
    LinearEnvelope,
    Clifford,
    DeJong,
    Tinkerbell,
    DiamondStack,
    CustomChord,
    MaurerRose,
    LissajousChord,
    SuperformulaChord,
};

struct ModularChordParams {
    int N = 200;
    double k = 2.0;
};

struct HypotrochoidParams {
    double R = 5.0;
    double r = 3.0;
    double d = 2.0;
    int samples = 4000;
};

struct EpitrochoidParams {
    double R = 3.0;
    double r = 1.0;
    double d = 1.5;
    int samples = 4000;
};

struct LissajousParams {
    double A = 1.0;
    double B = 1.0;
    double a = 3.0;
    double b = 2.0;
    double phi = std::numbers::pi / 2.0;
    int samples = 4000;
};

struct RoseParams {
    int n = 5;       // numerator of k = n/d in r = cos(k·θ)
    int d = 1;       // denominator
    int samples = 4000;
};

struct SuperformulaParams {
    double m = 5.0;
    double n1 = 2.0;
    double n2 = 7.0;
    double n3 = 7.0;
    double a = 1.0;
    double b = 1.0;
    int samples = 4000;
};

struct PhyllotaxisParams {
    int N = 500;
    double alpha = 2.39996322972865332;  // golden angle ≈ 137.508° in radians
    double k = 2.0;                      // modular chord multiplier
};

struct PolygonChordParams {
    int n_sides = 3;          // 3 = triangle, 4 = square, 5 = pentagon, ...
    int N = 200;              // points sampled along the perimeter
    double k = 2.0;           // modular chord multiplier
    double rotation_rad = 0.0;
};

struct LinearEnvelopeParams {
    // Default: perpendicular corner — vertical segment up from origin paired
    // with horizontal segment right from origin. With k=1 this gives the
    // canonical parabolic corner-fan envelope.
    Vec2 a_start = {0.0, 0.0};
    Vec2 a_end   = {0.0, 1.0};
    Vec2 b_start = {0.0, 0.0};
    Vec2 b_end   = {1.0, 0.0};
    int N = 30;
    double k = 1.0;
};

// Strange-attractor render-mode selector. Polyline = connect consecutive
// iterates with line segments (the original behaviour). Scatter = emit each
// iterate as a single point — the canonical scatter-plot attractor look,
// which reads as orbital density without the stroke texture from
// connect-consecutive-iterates. Both = emit polylines AND points.
enum class AttractorRenderMode {
    Polyline,
    Scatter,
    Both,
};

// Strange-attractor parameter blocks. Each holds the four map coefficients
// (a, b, c, d), the initial orbit point (x0, y0), and the
// burn_in / iterations counts. Defaults are tuned per attractor to land on
// the canonical aesthetic value (Clifford butterfly, de Jong classic,
// Tinkerbell basin).
struct CliffordParams {
    double a = -1.4;
    double b =  1.6;
    double c =  1.0;
    double d =  0.7;
    double x0 = 0.1;
    double y0 = 0.1;
    int iterations = 30000;
    int burn_in = 100;
    AttractorRenderMode render_mode = AttractorRenderMode::Polyline;
};

struct DeJongParams {
    double a =  1.4;
    double b = -2.3;
    double c =  2.4;
    double d = -2.1;
    double x0 = 0.1;
    double y0 = 0.1;
    int iterations = 30000;
    int burn_in = 100;
    AttractorRenderMode render_mode = AttractorRenderMode::Polyline;
};

struct TinkerbellParams {
    double a =  0.9;
    double b = -0.6013;
    double c =  2.0;
    double d =  0.5;
    double x0 = -0.72;
    double y0 = -0.64;
    int iterations = 10000;  // Tinkerbell escapes its basin readily; smaller default keeps it stable
    int burn_in = 100;
    AttractorRenderMode render_mode = AttractorRenderMode::Polyline;
};

// Which set of parabolic fans the diamond_stack generator emits per module.
//   Both       — all 4 (apexes at top, bottom, left, right tips)
//   Vertical   — only top+bottom apexes  → fills the top/bottom triangles
//   Horizontal — only left+right apexes  → fills the left/right triangles
// The Vertical/Horizontal split is the standard recipe for two-colour
// stacked-diamond compositions: one layer per fan set, each independently
// styled.
enum class DiamondStackFans {
    Both,
    Vertical,
    Horizontal,
};

// Maurer rose — N samples of r = sin(n·θ) at angular step `step_deg`,
// connected in order as a single polyline. With samples=360 and a coprime
// step (e.g. 71) the polyline shuffles through every sample and produces
// the dense fractal pattern that gives the figure its name.
struct MaurerRoseParams {
    int n = 7;
    int step_deg = 71;
    int samples = 360;
};

// Lissajous-chord — N nails sampled along a Lissajous curve, connected
// by the modular chord rule (i → round(k·i) mod N). Same a/b integer
// closure caveat as the plain Lissajous generator.
struct LissajousChordParams {
    double A = 1.0;
    double B = 1.0;
    int a = 3;        // integer for closure
    int b = 2;
    double phi = std::numbers::pi / 2.0;
    int N = 200;
    double k = 2.0;
};

// Superformula-chord — N nails sampled around a Gielis superformula curve,
// connected by the modular chord rule (i → round(k·i) mod N).
struct SuperformulaChordParams {
    double m  = 5.0;
    double n1 = 2.0;
    double n2 = 7.0;
    double n3 = 7.0;
    double a  = 1.0;
    double b  = 1.0;
    int N = 200;
    double k = 2.0;
};

// Explicit nail positions + chord pairs. Edited in-app via the canvas
// nail editor (left-click to place / connect), serialised to JSON, and
// reloaded losslessly. The escape hatch for hand-authored patterns.
//
// chord_colors is optional. When non-empty AND its size matches chords.size(),
// each entry overrides the layer style's colormap for that chord — i.e. the
// user can paint individual chords. When empty or mismatched, the style's
// colormap is used.
//
// chord_end_colors is optional. When non-empty AND its size matches
// chord_colors.size(), each chord renders as a gradient from chord_colors[i]
// (at the chord's first nail) to chord_end_colors[i] (at the second nail).
struct CustomChordParams {
    std::vector<Vec2> nails;
    std::vector<std::pair<int, int>> chords;
    std::vector<Color> chord_colors;
    std::vector<Color> chord_end_colors;
};

// Stacked hourglass / diamond string-art generator. Adjacent modules share
// tip points, producing the pinched-waist silhouette. See generators/
// diamond_stack.hpp for the chord-emission math.
struct DiamondStackParams {
    int    n_modules    = 3;    // stacked hourglass modules
    int    N            = 80;   // strings per fan
    double aspect       = 0.6;  // half_waist_width / half_module_height
    double rotation_rad = 0.0;
    DiamondStackFans fans = DiamondStackFans::Both;
};

struct GeneratorSpec {
    GeneratorType type = GeneratorType::ModularChord;
    ModularChordParams    chord;
    HypotrochoidParams    hypo;
    EpitrochoidParams     epi;
    LissajousParams       liss;
    RoseParams            rose;
    SuperformulaParams    supf;
    PhyllotaxisParams     phyl;
    PolygonChordParams    poly;
    LinearEnvelopeParams  lenv;
    CliffordParams        clif;
    DeJongParams          dejo;
    TinkerbellParams      tink;
    DiamondStackParams    dstack;
    CustomChordParams     custom;
    MaurerRoseParams      maurer;
    LissajousChordParams  lichord;
    SuperformulaChordParams supchord;
};

// ---------------------------------------------------------------------------
// Style spec (serializable; runtime equivalent is caustic::Style)

enum class ColorMapType {
    Solid,
    LinearGradient,
    HsvSweep,
    Diverging,
};

struct StyleSpec {
    ColorMapType colormap_type = ColorMapType::HsvSweep;

    Color solid_color    = {1.00, 1.00, 1.00, 1.0};
    Color gradient_start = {0.10, 0.27, 0.50, 1.0};
    Color gradient_end   = {0.94, 0.75, 0.31, 1.0};
    double hue_start = 0.0;
    double hue_end   = 360.0;
    double hsv_saturation = 0.85;
    double hsv_value      = 0.95;
    Color div_negative = {0.95, 0.55, 0.20, 1.0};
    Color div_midpoint = {0.95, 0.95, 0.95, 1.0};
    Color div_positive = {0.20, 0.70, 0.85, 1.0};

    Indexer color_indexer = Indexer::ChordLength;

    double stroke_width_min = 0.8;
    double stroke_width_max = 0.8;
    Indexer stroke_width_indexer = Indexer::ChordIndex;
    double opacity = 0.6;

    bool cyclic = false;
};

// ---------------------------------------------------------------------------
// Layer transform — applied to a layer's geometry pre-render.
//
// Application order (right-to-left composition): Mirror → Scale → Rotate → Translate.
// Equivalent to: v' = T + R(θ) · S · M · v.

struct LayerTransform {
    Vec2 translate = {0.0, 0.0};
    double rotate_rad = 0.0;
    double scale = 1.0;
    bool mirror_x = false;  // negate x (reflect across the y-axis)
    bool mirror_y = false;  // negate y (reflect across the x-axis)
};

inline Vec2 apply(LayerTransform t, Vec2 v) {
    if (t.mirror_x) v.x = -v.x;
    if (t.mirror_y) v.y = -v.y;
    v.x *= t.scale;
    v.y *= t.scale;
    const double c = std::cos(t.rotate_rad);
    const double s = std::sin(t.rotate_rad);
    const double x = v.x * c - v.y * s;
    const double y = v.x * s + v.y * c;
    return {x + t.translate.x, y + t.translate.y};
}

// ---------------------------------------------------------------------------
// Layer and Scene

struct Layer {
    std::string name;
    GeneratorSpec generator;
    StyleSpec style;
    LayerTransform transform;
    bool visible = true;
};

struct Scene {
    Color background = {0.04, 0.04, 0.04, 1.0};
    // Start with one empty editable layer so a default-constructed Preset has
    // something for the UI to bind to and tests can index scene.layers[0].
    std::vector<Layer> layers = {Layer{}};
};

// ---------------------------------------------------------------------------
// Top-level preset (v2 = scene-based; v1 readers still accepted via auto-promote
// in preset_io.hpp).

struct Preset {
    int version = 2;
    std::string name;
    Scene scene;
    CameraState camera;
};

}  // namespace caustic
