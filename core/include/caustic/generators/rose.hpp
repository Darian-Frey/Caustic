#pragma once

#include <cmath>
#include <cstdlib>
#include <numbers>
#include <numeric>
#include <utility>

#include <caustic/curve.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Rose curve (rhodonea): r(θ) = cos(n·θ / d) in polar, mapped to Cartesian
// as (r cos θ, r sin θ).
//
// For integer n, d the curve closes at θ = 2π·d (or π·d if n + d is odd,
// but sampling over 2π·d harmlessly retraces the same path).
//
//   n / d = 2          → quadrifolium (4-petal)
//   n / d = 3          → trifolium (3-petal)
//   n / d = 5/2        → 5-petal star with petals overlapping
//   n / d = 7/3        → 7-petal asymmetric rose
class RoseCurve : public ParametricCurve {
public:
    RoseCurve(int n, int d) : n_(n), d_(std::max(1, d)) {
        // Period 2π · d covers the full curve for any integer n, d.
        t_max_ = 2.0 * std::numbers::pi * static_cast<double>(d_);
        // |r| is bounded by 1 → bounding box is the unit square.
        extent_ = 1.0;
    }

    Vec2 evaluate(double t) const override {
        const double theta = t * t_max_;
        const double r = std::cos(static_cast<double>(n_) * theta / static_cast<double>(d_));
        return {r * std::cos(theta), r * std::sin(theta)};
    }

    std::pair<Vec2, Vec2> bounding_box() const override {
        return {{-extent_, -extent_}, {extent_, extent_}};
    }

private:
    int n_;
    int d_;
    double t_max_;
    double extent_;
};

}  // namespace caustic
