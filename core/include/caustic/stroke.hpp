#pragma once

#include <caustic/indexer.hpp>

namespace caustic {

struct StrokeStyle {
    double width_min = 1.0;
    double width_max = 1.0;       // equal → constant width
    Indexer width_indexer = Indexer::ChordIndex;
    double opacity = 1.0;         // multiplied with the colormap's alpha at draw time
};

}  // namespace caustic
