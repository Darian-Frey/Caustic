#pragma once

#include <vector>

#include <caustic/chord.hpp>
#include <caustic/color.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

struct GeometryBuffer {
    std::vector<std::vector<Vec2>> polylines;
    ChordSet chords;
    // Scatter geometry — one dot per entry. Used by the strange-attractor
    // generators in Scatter render mode so each iterate draws as a discrete
    // point instead of being connected by a polyline. Renderers iterate the
    // list and emit a small filled circle per point, coloured by the layer
    // style's color_map sampled at i / (N-1).
    std::vector<Vec2> points;
    // Optional per-chord colour override. Empty by default. When non-empty
    // AND its size matches chords.size(), each entry replaces the colour-map
    // sample for that chord. The custom_chord generator uses this so the
    // user can pick a colour per chord without having to set up a special
    // colormap. Other generators leave it empty.
    std::vector<Color> chord_color_overrides;       // start colour
    // Optional per-chord *end* colour. When this is non-empty AND its size
    // matches chord_color_overrides.size(), each chord renders as a gradient
    // from chord_color_overrides[i] (at endpoint a) to
    // chord_end_color_overrides[i] (at endpoint b). When equal to the start
    // colour, the renderer draws a single segment instead of subdividing.
    std::vector<Color> chord_end_color_overrides;
};

}  // namespace caustic
