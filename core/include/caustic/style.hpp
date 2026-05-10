#pragma once

#include <memory>

#include <caustic/color.hpp>
#include <caustic/colormap.hpp>
#include <caustic/indexer.hpp>
#include <caustic/stroke.hpp>

namespace caustic {

struct Style {
    std::shared_ptr<ColorMap> color_map;
    Indexer color_indexer = Indexer::ChordIndex;
    StrokeStyle stroke;
    Color background = {0.04, 0.04, 0.04, 1.0};
};

}  // namespace caustic
