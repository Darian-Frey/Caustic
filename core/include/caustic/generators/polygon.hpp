#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include <caustic/chord.hpp>
#include <caustic/curve.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Regular n-gon perimeter as a ParametricCurve. t ∈ [0, 1] is mapped to a
// constant-arc-length position around the perimeter: t·n picks the edge,
// the fractional part interpolates along it. Vertices sit on the unit circle.
class PolygonCurve : public ParametricCurve {
public:
    PolygonCurve(int n_sides, double rotation_rad = 0.0)
        : n_(std::max(3, n_sides)), rot_(rotation_rad) {}

    Vec2 evaluate(double t) const override {
        const double scaled = t * static_cast<double>(n_);
        const double floor_v = std::floor(scaled);
        int edge_idx = static_cast<int>(floor_v);
        edge_idx = ((edge_idx % n_) + n_) % n_;
        const double frac = scaled - floor_v;
        const Vec2 a = vertex(edge_idx);
        const Vec2 b = vertex((edge_idx + 1) % n_);
        return {a.x + (b.x - a.x) * frac, a.y + (b.y - a.y) * frac};
    }

    std::pair<Vec2, Vec2> bounding_box() const override {
        return {{-1.0, -1.0}, {1.0, 1.0}};
    }

    int sides() const { return n_; }

private:
    Vec2 vertex(int k) const {
        const double theta = 2.0 * std::numbers::pi * static_cast<double>(k)
                             / static_cast<double>(n_) + rot_;
        return {std::cos(theta), std::sin(theta)};
    }

    int n_;
    double rot_;
};

// Polygon-vertex modular chord: sample N points uniformly along an n-gon's
// perimeter, then apply the modular chord rule chord_i: P_i → P_{round(k·i) mod N}.
//
// Classical string-art family. With N a multiple of n_sides and small integer k,
// you get the canonical deltoid envelope (n_sides=3, k=2), hexagram (n_sides=6,
// k=2), and so on.
inline ChordSet polygon_chord(int n_sides, int N, double k, double rotation_rad = 0.0) {
    ChordSet chords;
    if (N <= 0 || n_sides < 3) return chords;
    chords.reserve(static_cast<std::size_t>(N));

    PolygonCurve curve(n_sides, rotation_rad);
    std::vector<Vec2> points;
    points.reserve(static_cast<std::size_t>(N));
    for (int i = 0; i < N; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(N);
        points.push_back(curve.evaluate(t));
    }

    for (int i = 0; i < N; ++i) {
        const long long j_raw = static_cast<long long>(std::lround(k * static_cast<double>(i)));
        const int j = static_cast<int>(((j_raw % N) + N) % N);
        const double t_along = static_cast<double>(i) / static_cast<double>(N);
        chords.push_back({points[i], points[j], t_along});
    }

    return chords;
}

}  // namespace caustic
