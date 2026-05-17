#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include <caustic/chord.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Superformula-chord — N nail points sampled around a Gielis superformula
// curve, then connected by the modular chord rule i → round(k·i) mod N.
//
// Gielis formula (one common form):
//   r(θ) = ( |cos(m·θ/4) / a|^n2 + |sin(m·θ/4) / b|^n3 )^(-1/n1)
// and the cartesian point is (r·cos(θ), r·sin(θ)).
//
// Defaults (m=5, n1=2, n2=7, n3=7, a=1, b=1) give the classic 5-pointed
// starfish — visually striking when overlaid with chord rule k=2 or k=3.

inline ChordSet superformula_chord(double m, double n1, double n2, double n3,
                                    double a, double b, int N, double k) {
    ChordSet chords;
    if (N <= 0) return chords;
    chords.reserve(static_cast<std::size_t>(N));

    auto superformula_r = [&](double theta) -> double {
        const double q  = m * theta / 4.0;
        const double t1 = std::pow(std::abs(std::cos(q) / a), n2);
        const double t2 = std::pow(std::abs(std::sin(q) / b), n3);
        const double s  = t1 + t2;
        if (s <= 0.0) return 0.0;
        return std::pow(s, -1.0 / n1);
    };

    std::vector<Vec2> pts;
    pts.reserve(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        const double theta = 2.0 * std::numbers::pi *
                             static_cast<double>(i) / static_cast<double>(N);
        const double r = superformula_r(theta);
        pts.push_back({r * std::cos(theta), r * std::sin(theta)});
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
