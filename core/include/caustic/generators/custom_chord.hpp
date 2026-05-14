#pragma once

#include <cstddef>
#include <utility>
#include <vector>

#include <caustic/chord.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Custom chord set — explicit nail positions and explicit chord pairs.
//
// The "escape hatch" generator for hand-authored string-art patterns
// that don't reduce to a single closed-form rule. Each nail is a 2D point;
// each chord connects two nails by index. Indices that are out of range
// are silently skipped (defensive — survives partial edits).
//
// Authoring is intended via the in-app canvas nail editor, but the JSON
// schema is hand-editable too:
//
//   {
//     "type": "custom_chord",
//     "params": {
//       "nails":  [[0.0, 1.0], [0.7, 0.7], ...],
//       "chords": [[0, 4], [1, 5], ...]
//     }
//   }
//
// t_along is assigned as chord_index / chord_count so the existing colour
// indexers (by_chord_index, by_curve_t) sweep through the chords in
// emission order — paint chords in the order you want the colour gradient
// to run.

inline ChordSet custom_chord(const std::vector<Vec2>& nails,
                              const std::vector<std::pair<int, int>>& chord_pairs) {
    ChordSet chords;
    const std::size_t n = nails.size();
    if (n == 0 || chord_pairs.empty()) return chords;
    chords.reserve(chord_pairs.size());

    const double denom = static_cast<double>(chord_pairs.size());
    for (std::size_t i = 0; i < chord_pairs.size(); ++i) {
        const int a = chord_pairs[i].first;
        const int b = chord_pairs[i].second;
        if (a < 0 || a >= static_cast<int>(n)) continue;
        if (b < 0 || b >= static_cast<int>(n)) continue;
        chords.push_back({nails[static_cast<std::size_t>(a)],
                          nails[static_cast<std::size_t>(b)],
                          static_cast<double>(i) / denom});
    }
    return chords;
}

}  // namespace caustic
