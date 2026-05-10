#pragma once

#include <vector>

#include <caustic/chord.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

struct GeometryBuffer {
    std::vector<std::vector<Vec2>> polylines;
    ChordSet chords;
};

}  // namespace caustic
