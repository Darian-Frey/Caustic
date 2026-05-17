#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include <caustic/chord.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Lissajous-chord — N nail points sampled along a Lissajous curve
//   P(t) = (A·sin(a·t + φ), B·sin(b·t)),   t ∈ [0, 2π)
// with the modular chord rule chord_i: P_i → P_{round(k·i) mod N}.
//
// Integer a / b for closed curves; non-integer values render but the curve
// won't close, so the chord pattern won't perfectly tile. Same closure
// caveat as the plain Lissajous generator.
//
// Default a=3, b=2, φ=π/2 gives the classic 3:2 Lissajous "tied bow".
// With k = 2 the chord pattern adds a cardioid-style cusp envelope on top.

inline ChordSet lissajous_chord(double A, double B,
                                 double a_freq, double b_freq,
                                 double phi, int N, double k) {
    ChordSet chords;
    if (N <= 0) return chords;
    chords.reserve(static_cast<std::size_t>(N));

    std::vector<Vec2> pts;
    pts.reserve(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        const double t = 2.0 * std::numbers::pi * static_cast<double>(i) /
                         static_cast<double>(N);
        pts.push_back({A * std::sin(a_freq * t + phi),
                       B * std::sin(b_freq * t)});
    }

    for (int i = 0; i < N; ++i) {
        const long long j_raw = static_cast<long long>(std::lround(k * static_cast<double>(i)));
        const int j = static_cast<int>(((j_raw % N) + N) % N);
        const double t_along = static_cast<double>(i) / static_cast<double>(N);
        chords.push_back({pts[i], pts[j], t_along});
    }
    return chords;
}

}  // namespace caustic
