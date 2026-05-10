#pragma once

#include <vector>

#include <caustic/vec2.hpp>

namespace caustic {

struct Chord {
    Vec2 a;
    Vec2 b;
    double t_along;  // i / N for color/width indexing
};

using ChordSet = std::vector<Chord>;

}  // namespace caustic
