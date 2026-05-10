#pragma once

#include <cmath>
#include <cstdlib>
#include <numbers>
#include <numeric>
#include <utility>

#include <caustic/curve.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Small circle radius r rolls outside big circle radius R, pen offset d.
//   x(τ) = (R + r) cos(τ) - d cos((R + r) / r · τ)
//   y(τ) = (R + r) sin(τ) - d sin((R + r) / r · τ)
//
// Closure period T = 2π · r / gcd(R, r) for integer R, r — see hypotrochoid
// header for the non-integer caveat.
class EpitrochoidCurve : public ParametricCurve {
public:
    EpitrochoidCurve(double R, double r, double d) : R_(R), r_(r), d_(d) {
        const long Ri = std::abs(static_cast<long>(std::lround(R)));
        const long ri = std::abs(static_cast<long>(std::lround(r)));
        const long g = std::gcd(Ri, ri);
        if (g == 0 || ri == 0) {
            t_max_ = 2.0 * std::numbers::pi;
        } else {
            t_max_ = 2.0 * std::numbers::pi * static_cast<double>(ri) / static_cast<double>(g);
        }
    }

    Vec2 evaluate(double t) const override {
        const double tau = t * t_max_;
        const double sum = R_ + r_;
        const double k = sum / r_;
        return {
            sum * std::cos(tau) - d_ * std::cos(k * tau),
            sum * std::sin(tau) - d_ * std::sin(k * tau),
        };
    }

    std::pair<Vec2, Vec2> bounding_box() const override {
        const double extent = std::abs(R_ + r_) + std::abs(d_);
        return {{-extent, -extent}, {extent, extent}};
    }

private:
    double R_;
    double r_;
    double d_;
    double t_max_;
};

}  // namespace caustic
