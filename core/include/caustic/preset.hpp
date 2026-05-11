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
