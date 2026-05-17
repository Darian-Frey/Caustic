#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include <caustic/vec2.hpp>

namespace caustic {

// Maurer rose — the classic "rose curve revisited" pattern. A normal rose is
// r(θ) = sin(n·θ) traced continuously. A Maurer rose samples the rose at
// integer-degree angles step_deg·k (k = 0..N-1) and connects consecutive
// samples with straight line segments. With N = 360 and a coprime step_deg
// (e.g. 71, 67, 47), the resulting polyline visits every sample point in a
// shuffled order, producing the dense fractal-feeling chord pattern that
// makes the figure famous.
//
// Parameters
// ----------
// n        — petal-frequency of the underlying rose r = sin(n·θ).
//            Odd n → n petals; even n → 2n petals; negative ignored.
// step_deg — angular step between consecutive samples, in degrees.
//            Use a value coprime with N (e.g. 71 with N=360) for the
//            characteristic Maurer interleave.
// samples  — number of points to visit. 360 is the canonical default and
//            guarantees the polyline closes for any integer step_deg.
//
// Output is a single closed polyline (last point repeats the first), suitable
// for the polyline pipeline tier.

inline std::vector<Vec2> maurer_rose(int n, int step_deg, int samples) {
    std::vector<Vec2> pts;
    if (n <= 0 || samples < 2) return pts;
    pts.reserve(static_cast<std::size_t>(samples) + 1);
    const double step_rad = static_cast<double>(step_deg) * std::numbers::pi / 180.0;
    for (int k = 0; k <= samples; ++k) {
        const double theta = static_cast<double>(k) * step_rad;
        const double r     = std::sin(static_cast<double>(n) * theta);
        pts.push_back({r * std::cos(theta), r * std::sin(theta)});
    }
    return pts;
}

}  // namespace caustic
