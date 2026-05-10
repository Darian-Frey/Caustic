#pragma once

#include <memory>

#include <caustic/color.hpp>
#include <caustic/colormap.hpp>
#include <caustic/indexer.hpp>
#include <caustic/stroke.hpp>

namespace caustic {

// Per-layer styling. Background is now scene-level (caustic::Scene.background)
// and passed to renderers as a separate parameter, so it's not a Style field.
struct Style {
    std::shared_ptr<ColorMap> color_map;
    Indexer color_indexer = Indexer::ChordIndex;
    StrokeStyle stroke;

    // For closed curves: remap t through a triangle wave (1 - |2t - 1|) so
    // the colormap and stroke width return to their starting value at t=1,
    // hiding the seam where the curve loops back. Default off — modular chord
    // and other open/discrete primitives don't need it.
    bool cyclic = false;
};

}  // namespace caustic
