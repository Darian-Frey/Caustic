#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>

#include <caustic/chord.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Modular chord rule on N equispaced points of the unit circle.
//   P_i    = (cos(2π i / N), sin(2π i / N))
//   chord_i: P_i → P_{round(k · i) mod N}
//
// k is allowed to be non-integer — the curve morphs continuously between integer
// values (k = 2 → cardioid, k = 3 → nephroid, k = 2.5 → halfway between).
inline ChordSet modular_chord(int N, double k) {
    ChordSet chords;
    if (N <= 0) return chords;
    chords.reserve(static_cast<std::size_t>(N));

    const double two_pi_over_N = 2.0 * std::numbers::pi / static_cast<double>(N);

    for (int i = 0; i < N; ++i) {
        const double angle_i = two_pi_over_N * i;
        const Vec2 a{std::cos(angle_i), std::sin(angle_i)};

        const long long j_raw = static_cast<long long>(std::lround(k * i));
        // C++ % gives a negative remainder for negative dividends; normalise into [0, N).
        const int j = static_cast<int>(((j_raw % N) + N) % N);
        const double angle_j = two_pi_over_N * j;
        const Vec2 b{std::cos(angle_j), std::sin(angle_j)};

        const double t_along = static_cast<double>(i) / static_cast<double>(N);
        chords.push_back({a, b, t_along});
    }

    return chords;
}

}  // namespace caustic
