#pragma once

#include <cmath>
#include <cstddef>
#include <vector>

#include <caustic/vec2.hpp>

namespace caustic {

// Strange-attractor orbits. Each map is iterated from (x_0, y_0) for
// `burn_in + iterations` steps; the first `burn_in` are discarded so the
// recorded polyline lies on the attractor rather than the transient.
//
// The Tinkerbell map (and any chaotic system perturbed off the canonical
// parameters) can diverge — when |x| or |y| exceeds a sentinel limit, the
// orbit is truncated and `diverged` is set so the renderer / UI can warn
// instead of emitting broken geometry.
//
// Output is the iteration sequence as a polyline, ordered by iteration
// index. Caustic's existing CurveT indexer already produces chronological
// colouring (i / (N - 1)), so no new indexer is needed.

struct AttractorOrbit {
    std::vector<Vec2> points;
    bool diverged = false;
};

namespace detail {

constexpr double kAttractorDivergeLimit = 1.0e6;

inline bool orbit_diverged(double x, double y) {
    return !std::isfinite(x) || !std::isfinite(y)
        || std::abs(x) > kAttractorDivergeLimit
        || std::abs(y) > kAttractorDivergeLimit;
}

}  // namespace detail

// Iterate a step function `step(x, y, nx, ny)` from (x0, y0). Skip
// `burn_in` transient steps, then record `iterations` points. Aborts
// early on divergence.
template <typename Step>
inline AttractorOrbit iterate_orbit(double x0, double y0,
                                    int burn_in, int iterations,
                                    Step step) {
    AttractorOrbit out;
    if (iterations <= 0) return out;
    out.points.reserve(static_cast<std::size_t>(iterations));

    double x = x0, y = y0;
    for (int i = 0; i < burn_in; ++i) {
        double nx = 0.0, ny = 0.0;
        step(x, y, nx, ny);
        x = nx; y = ny;
        if (detail::orbit_diverged(x, y)) {
            out.diverged = true;
            return out;
        }
    }
    for (int i = 0; i < iterations; ++i) {
        out.points.push_back({x, y});
        double nx = 0.0, ny = 0.0;
        step(x, y, nx, ny);
        x = nx; y = ny;
        if (detail::orbit_diverged(x, y)) {
            out.diverged = true;
            break;
        }
    }
    return out;
}

// Clifford Pickover attractor:
//   x_{n+1} = sin(a·y_n) + c·cos(a·x_n)
//   y_{n+1} = sin(b·x_n) + d·cos(b·y_n)
// Canonical butterfly: (a, b, c, d) = (-1.4, 1.6, 1.0, 0.7).
inline AttractorOrbit clifford_orbit(double a, double b, double c, double d,
                                     double x0, double y0,
                                     int burn_in, int iterations) {
    return iterate_orbit(x0, y0, burn_in, iterations,
        [a, b, c, d](double x, double y, double& nx, double& ny) {
            nx = std::sin(a * y) + c * std::cos(a * x);
            ny = std::sin(b * x) + d * std::cos(b * y);
        });
}

// Peter de Jong attractor:
//   x_{n+1} = sin(a·y_n) − cos(b·x_n)
//   y_{n+1} = sin(c·x_n) − cos(d·y_n)
// Classic: (a, b, c, d) = (1.4, −2.3, 2.4, −2.1).
inline AttractorOrbit de_jong_orbit(double a, double b, double c, double d,
                                    double x0, double y0,
                                    int burn_in, int iterations) {
    return iterate_orbit(x0, y0, burn_in, iterations,
        [a, b, c, d](double x, double y, double& nx, double& ny) {
            nx = std::sin(a * y) - std::cos(b * x);
            ny = std::sin(c * x) - std::cos(d * y);
        });
}

// Tinkerbell map:
//   x_{n+1} = x_n² − y_n² + a·x_n + b·y_n
//   y_{n+1} = 2·x_n·y_n + c·x_n + d·y_n
// Canonical: (a, b, c, d) = (0.9, −0.6013, 2.0, 0.5), (x0, y0) = (−0.72, −0.64).
// This map readily diverges if the basin is escaped; iterate_orbit handles
// that by truncating and setting `diverged = true`.
inline AttractorOrbit tinkerbell_orbit(double a, double b, double c, double d,
                                      double x0, double y0,
                                      int burn_in, int iterations) {
    return iterate_orbit(x0, y0, burn_in, iterations,
        [a, b, c, d](double x, double y, double& nx, double& ny) {
            nx = x * x - y * y + a * x + b * y;
            ny = 2.0 * x * y + c * x + d * y;
        });
}

}  // namespace caustic
