#pragma once

#include <cmath>
#include <cstdlib>
#include <numbers>
#include <numeric>
#include <utility>

#include <caustic/curve.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Two perpendicular sinusoidal oscillators.
//   x(τ) = A sin(a τ + φ)
//   y(τ) = B sin(b τ)
//
// Closure period T = 2π / gcd(a, b) for integer a, b — for irrational a/b the
// curve precesses indefinitely.
class LissajousCurve : public ParametricCurve {
public:
    LissajousCurve(double A, double B, double a, double b, double phi)
        : A_(A), B_(B), a_(a), b_(b), phi_(phi) {
        const long ai = std::abs(static_cast<long>(std::lround(a)));
        const long bi = std::abs(static_cast<long>(std::lround(b)));
        const long g = std::gcd(ai, bi);
        if (g == 0) {
            t_max_ = 2.0 * std::numbers::pi;
        } else {
            t_max_ = 2.0 * std::numbers::pi / static_cast<double>(g);
        }
    }

    Vec2 evaluate(double t) const override {
        const double tau = t * t_max_;
        return {
            A_ * std::sin(a_ * tau + phi_),
            B_ * std::sin(b_ * tau),
        };
    }

    std::pair<Vec2, Vec2> bounding_box() const override {
        return {{-std::abs(A_), -std::abs(B_)}, {std::abs(A_), std::abs(B_)}};
    }

private:
    double A_;
    double B_;
    double a_;
    double b_;
    double phi_;
    double t_max_;
};

}  // namespace caustic
