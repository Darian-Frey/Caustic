#pragma once

#include <cmath>
#include <cstddef>

#include <caustic/chord.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Linear two-segment envelope — the classical "thread and nails" string art.
//
// Place N points evenly along segment A (a_start → a_end) and N along segment
// B (b_start → b_end). Connect A's i-th point to B's round(k·i) mod N-th.
//
// The envelope of the chord family is a conic section determined by the two
// segments' geometry and k:
//   • A and B perpendicular meeting at a corner, k=1 → parabolic corner fan
//   • A and B parallel, k=-1                       → bowtie (X) figure
//   • Other geometries / k values                  → richer caustic curves
//
// Different topology from modular chord on a circle: chord endpoints lie on
// two distinct line segments instead of on one closed curve. Returns a
// ChordSet that styles and renders identically to the other chord-set
// generators (modular chord, phyllotaxis_chord, polygon_chord).
inline ChordSet linear_envelope(Vec2 a_start, Vec2 a_end,
                                 Vec2 b_start, Vec2 b_end,
                                 int N, double k) {
    ChordSet chords;
    if (N <= 0) return chords;
    chords.reserve(static_cast<std::size_t>(N));

    const double denom = (N > 1) ? static_cast<double>(N - 1) : 1.0;

    auto lerp = [](Vec2 p, Vec2 q, double t) -> Vec2 {
        return {p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t};
    };

    for (int i = 0; i < N; ++i) {
        const double t_a = static_cast<double>(i) / denom;
        const Vec2 A = lerp(a_start, a_end, t_a);

        const long long j_raw = static_cast<long long>(std::lround(k * static_cast<double>(i)));
        const int j = static_cast<int>(((j_raw % N) + N) % N);
        const double t_b = (N > 1) ? static_cast<double>(j) / denom : 0.0;
        const Vec2 B = lerp(b_start, b_end, t_b);

        const double t_along = static_cast<double>(i) / static_cast<double>(N);
        chords.push_back({A, B, t_along});
    }

    return chords;
}

}  // namespace caustic
