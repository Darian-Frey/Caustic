#pragma once

// G-code (Grbl flavour) and HPGL emitters for pen plotters. Sit alongside
// svg_renderer in caustic-render-svg — no raylib dependency, fully headless,
// so the CLI links them in for free. Same input → byte-identical output
// (fixed precision, deterministic chord ordering).
//
// Both formats consume a `vector<LayerRender>` (the same input svg_renderer
// takes) and emit a single-pen, single-colour drawing — line-art is the
// natural plotter idiom anyway. Scatter geometry (attractor points) is
// skipped because plotters can't draw a true zero-length point.
//
// Coordinate model: every output uses math-up (+y up), fit-to-page with a
// configurable margin and origin at the page centre. G-code coords are mm;
// HPGL coords are plotter units (40 per mm — the HP standard).

#include <filesystem>
#include <string>
#include <vector>

#include <caustic/scene_render.hpp>

namespace caustic {

struct PlotterOptions {
    // Page / bed dimensions in millimetres. The drawing fits inside the page
    // with the configured margin, preserving aspect ratio.
    double width_mm  = 200.0;
    double height_mm = 200.0;
    // Margin as a fraction of min(width_mm, height_mm). 0.05 = 5% all sides.
    double margin    = 0.05;

    // G-code-only knobs.
    double pen_up_z         =  5.0;     // mm — Z height with pen lifted
    double pen_down_z       =  0.0;     // mm — Z height with pen touching
    double travel_feedrate  = 6000.0;   // mm/min — rapid moves
    double draw_feedrate    = 3000.0;   // mm/min — drawing
    double plunge_feedrate  = 1500.0;   // mm/min — Z-axis pen up/down moves

    // HPGL-only knobs.
    int    pen_number       = 1;        // SP<n>; — which pen to select
};

// Render to a G-code string (Grbl flavour: G21/G90/G17/G94 header, G0/G1
// motion, Z pen control, M2 end). Travel order: chords / polylines are
// lexicographically sorted by start point — a cheap nearest-neighbour
// approximation that matches svg_renderer's plotter mode.
std::string render_gcode(const std::vector<LayerRender>& layers,
                         const PlotterOptions& opts = {});

// Render to an HPGL string (IN; / SP<n>; header, PU/PD/PA motion, IN; end).
// Plotter-unit coordinates at 40 PU/mm. Single pen — multi-pen support is
// a future polish.
std::string render_hpgl(const std::vector<LayerRender>& layers,
                        const PlotterOptions& opts = {});

// Convenience: render + write to disk, creating parent directories.
void write_gcode(const std::filesystem::path& path,
                 const std::vector<LayerRender>& layers,
                 const PlotterOptions& opts = {});

void write_hpgl(const std::filesystem::path& path,
                const std::vector<LayerRender>& layers,
                const PlotterOptions& opts = {});

}  // namespace caustic
