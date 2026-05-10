#pragma once

#include <numbers>
#include <string>

#include <caustic/camera.hpp>
#include <caustic/color.hpp>
#include <caustic/indexer.hpp>

namespace caustic {

// ---------------------------------------------------------------------------
// Generator specs

enum class GeneratorType {
    ModularChord,
    Hypotrochoid,
    Epitrochoid,
    Lissajous,
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

// Tagged union of generator parameters. Only the field matching `type` is
// load-bearing for any given preset; the others hold defaults.
struct GeneratorSpec {
    GeneratorType type = GeneratorType::ModularChord;
    ModularChordParams chord;
    HypotrochoidParams hypo;
    EpitrochoidParams  epi;
    LissajousParams    liss;
};

// ---------------------------------------------------------------------------
// Style spec (serializable; runtime equivalent is caustic::Style with a
// constructed shared_ptr<ColorMap>)

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

    Color background = {0.04, 0.04, 0.04, 1.0};

    bool cyclic = false;
};

// ---------------------------------------------------------------------------
// Top-level preset

struct Preset {
    int version = 1;
    std::string name;
    GeneratorSpec generator;
    StyleSpec style;
    CameraState camera;
};

}  // namespace caustic
