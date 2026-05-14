#pragma once

#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include <caustic/chord.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Diamond stack — stacked hourglass / bowtie string art.
//
// Each "module" is a diamond shape with four anchor points:
//   T  = top tip    (on vertical axis, above centre)
//   B  = bottom tip (on vertical axis, below centre)
//   L  = left waist (at the equator, left of axis)
//   R  = right waist (at the equator, right of axis)
//
// Two parabolic fans per module are generated:
//   Upper fan: points along T→L connect to points along T→R (reversed index).
//              Envelope is a parabola with vertex at T, opening downward.
//   Lower fan: points along B→L connect to points along B→R (reversed index).
//              Envelope is a parabola with vertex at B, opening upward.
//
// Adjacent modules share their T/B tip points — module i's B is module i+1's T —
// so the stack "pinches" at each junction, producing the characteristic hourglass
// waist visible in the reference images.
//
// Coordinate frame: the whole stack is centred at the origin and normalised so its
// total height is 1.0.  The renderer's fit-to-content scaling handles the rest.
//
// Parameters
// ----------
// n_modules   — number of stacked modules (≥ 1).
// N           — strings per fan (≥ 2).  Visual sweet-spot: 40–120.
// aspect      — half_waist_width / half_module_height.
//               < 1  → tall, thin diamonds (needle-like).
//               = 1  → 45° diamond.
//               > 1  → wide, flat modules.
//               Default 0.6 matches the proportions in the reference images.
// rotation_rad — rotate the entire stack around the origin before returning.
//               Compose stacks at different angles (0, π/2, π/3 …) with
//               multi-layer scenes + rotational_array for the radial star variants.
//
// Coloring
// --------
// t_along is assigned sequentially across all chords in emission order
// (upper fan then lower fan, module 0 then module 1 …).  This means:
//   • by_chord_index  → gradient sweeping top-to-bottom through the stack.
//   • by_chord_length → short chords (near tips) vs long chords (near waist).
//   • by_angle        → direction-coloured, highlights the X-crossing at each waist.
//
// Multi-layer recipes for the reference images
// --------------------------------------------
// Image 1 (red + white two-colour hourglass):
//   Layer 0: DiamondStack, n_modules=3, aspect=0.55, colour white,  opacity 0.7
//   Layer 1: DiamondStack, n_modules=3, aspect=0.35, colour red,    opacity 0.8
//
// Image 2 (neon star of stacks):
//   Layer 0: DiamondStack, n_modules=5, rotation=0,      colour cyan
//   Layer 1: DiamondStack, n_modules=5, rotation=π/2,    colour orange
//   Layer 2: DiamondStack, n_modules=5, rotation=π/4,    colour magenta
//   (or use Layers panel → Apply rotational array with N=4 on a single stack)
//
// Image 4 (2×3 grid of square-chord patterns):
//   polygon_chord n_sides=4, k=2  →  Apply grid tile 2 rows × 3 cols

inline ChordSet diamond_stack(int n_modules, int N,
                               double aspect     = 0.6,
                               double rotation_rad = 0.0) {
    if (n_modules <= 0 || N < 2) return {};

    // Two fans (upper + lower) × N chords each, per module.
    ChordSet chords;
    chords.reserve(static_cast<std::size_t>(n_modules) * 2 * static_cast<std::size_t>(N));

    const double module_height = 1.0 / static_cast<double>(n_modules);
    const double half_width    = aspect * module_height * 0.5;
    const double denom         = static_cast<double>(N - 1);

    // Optional rotation applied to every point.
    const double cos_r = std::cos(rotation_rad);
    const double sin_r = std::sin(rotation_rad);
    auto rot = [&](Vec2 v) -> Vec2 {
        return {v.x * cos_r - v.y * sin_r,
                v.x * sin_r + v.y * cos_r};
    };

    // Linear interpolation between two 2D points.
    auto lp = [](Vec2 p, Vec2 q, double t) -> Vec2 {
        return {p.x + (q.x - p.x) * t,
                p.y + (q.y - p.y) * t};
    };

    // Collect raw endpoint pairs, then assign t_along in one pass so that
    // coloring by chord_index produces a top-to-bottom sweep.
    std::vector<std::pair<Vec2, Vec2>> raw;
    raw.reserve(static_cast<std::size_t>(n_modules) * 2 * static_cast<std::size_t>(N));

    for (int m = 0; m < n_modules; ++m) {
        // Stack spans y ∈ [−0.5, +0.5], +y up (math-up convention).
        const double y_top = 0.5 - static_cast<double>(m) * module_height;
        const double y_bot = y_top - module_height;
        const double y_mid = (y_top + y_bot) * 0.5;

        const Vec2 T{0.0, y_top};
        const Vec2 B{0.0, y_bot};
        const Vec2 L{-half_width, y_mid};
        const Vec2 R{+half_width, y_mid};

        // Parabolic fan: N strings connecting arm_A[i] to arm_B[N-1-i].
        // The reversal is what creates the parabolic envelope — without it
        // you get straight fills rather than curves.
        auto emit_fan = [&](Vec2 apex, Vec2 left_base, Vec2 right_base) {
            for (int i = 0; i < N; ++i) {
                const double t  = static_cast<double>(i) / denom;
                const double rt = 1.0 - t;
                raw.emplace_back(rot(lp(apex, left_base,  t)),
                                 rot(lp(apex, right_base, rt)));
            }
        };

        emit_fan(T, L, R);  // upper parabola, vertex at top tip
        emit_fan(B, L, R);  // lower parabola, vertex at bottom tip
    }

    // Assign t_along now that we know the total chord count.
    const double total = static_cast<double>(raw.size());
    for (std::size_t i = 0; i < raw.size(); ++i) {
        chords.push_back({raw[i].first, raw[i].second,
                          static_cast<double>(i) / total});
    }

    return chords;
}

}  // namespace caustic
