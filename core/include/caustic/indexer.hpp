#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>

#include <caustic/vec2.hpp>

namespace caustic {

// Mapping strategy from a primitive (chord or polyline segment) to t ∈ [0, 1],
// fed into both ColorMap and stroke width interpolation.
enum class Indexer {
    ChordIndex,    // i / (N - 1)
    ChordLength,   // |b - a| / max_length
    Angle,         // (atan2(b.y - a.y, b.x - a.x) + π) / (2π)
    CurveT,        // i / (N - 1) — same as ChordIndex for our uniform samplers,
                   //               kept distinct for spec/preset clarity
};

// Compute the indexer's t value for a single segment a→b at index i in a set
// of N primitives. `max_length` is the longest segment in the set, only used
// by the ChordLength indexer; pass 0 if not applicable.
inline double indexer_value(Indexer ind, Vec2 a, Vec2 b,
                            std::size_t i, std::size_t N,
                            double max_length) {
    switch (ind) {
        case Indexer::ChordIndex:
        case Indexer::CurveT: {
            if (N <= 1) return 0.0;
            return static_cast<double>(i) / static_cast<double>(N - 1);
        }
        case Indexer::ChordLength: {
            if (max_length <= 0.0) return 0.0;
            const double dx = b.x - a.x;
            const double dy = b.y - a.y;
            return std::sqrt(dx * dx + dy * dy) / max_length;
        }
        case Indexer::Angle: {
            const double theta = std::atan2(b.y - a.y, b.x - a.x);
            return (theta + std::numbers::pi) / (2.0 * std::numbers::pi);
        }
    }
    return 0.0;
}

}  // namespace caustic
