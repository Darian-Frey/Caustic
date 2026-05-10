#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>

#include <caustic/chord.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Phyllotaxis point disk: P_i = (√i · cos(i·α), √i · sin(i·α)) for i ∈ [0, N).
// The default α = 2π/φ² ≈ 137.508° is the golden angle, which packs points
// into the sunflower-seed spiral pattern.
//
// This generator returns the chord variant — string art on the disk by
// applying the modular chord rule chord_i: P_i → P_{round(k·i) mod N} on
// the N phyllotaxis points. The visual is two-fold spirals (from φ²) with
// the chord pattern overlaid.
//
// k = 0   → all chords radiate from P_0
// k = 1   → degenerate, every chord is zero-length
// k = 2.5 → mid-morph between integer multipliers (caustic-style sweep)
//
// Output coordinates aren't bounded by a fixed extent — the disk grows as
// √N. The renderer's fit-to-content scaling handles it.
inline ChordSet phyllotaxis_chord(int N, double alpha, double k) {
    ChordSet chords;
    if (N <= 0) return chords;
    chords.reserve(static_cast<std::size_t>(N));

    // Pre-compute all N points.
    std::vector<Vec2> points;
    points.reserve(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        const double r = std::sqrt(static_cast<double>(i));
        const double theta = static_cast<double>(i) * alpha;
        points.push_back({r * std::cos(theta), r * std::sin(theta)});
    }

    // Apply modular chord rule.
    for (int i = 0; i < N; ++i) {
        const long long j_raw = static_cast<long long>(std::lround(k * i));
        const int j = static_cast<int>(((j_raw % N) + N) % N);
        const double t_along = static_cast<double>(i) / static_cast<double>(N);
        chords.push_back({points[i], points[j], t_along});
    }

    return chords;
}

// Golden angle in radians: 2π / φ² ≈ 137.508°.
inline constexpr double golden_angle_rad() {
    // φ = (1 + √5) / 2; φ² = (3 + √5) / 2.
    // Returning the numerical value directly to keep it constexpr-friendly
    // without involving std::sqrt at compile time.
    return 2.39996322972865332;
}

}  // namespace caustic
