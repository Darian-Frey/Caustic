#pragma once

#include <cmath>
#include <numbers>
#include <utility>

#include <caustic/curve.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Gielis's superformula in polar form. One equation, six parameters:
//
//   r(φ) = ( |cos(m·φ/4) / a|^n2 + |sin(m·φ/4) / b|^n3 )^(-1/n1)
//   x(φ) = r(φ) cos(φ)
//   y(φ) = r(φ) sin(φ)
//
// m controls rotational symmetry. Integer m closes at 2π.
// n1 / n2 / n3 control shape exponents.
// a / b are axis scales (default 1, 1).
//
// Examples:
//   (m=0, n1=n2=n3=1)             → circle
//   (m=4, n1=n2=n3=100, a=b=1)    → rounded square
//   (m=5, n1=2, n2=7, n3=7)       → starfish
//   (m=6, n1=1, n2=1, n3=1)       → hexagonal flower
class SuperformulaCurve : public ParametricCurve {
public:
    SuperformulaCurve(double m, double n1, double n2, double n3, double a, double b)
        : m_(m), n1_(std::max(1e-6, n1)), n2_(n2), n3_(n3),
          a_(std::max(1e-6, std::abs(a))), b_(std::max(1e-6, std::abs(b))) {
        t_max_ = 2.0 * std::numbers::pi;
        // r is at most max(1/a, 1/b)^(1/n1) when one of cos/sin is zero.
        // Compute a safe upper-bound for the bounding box.
        const double r_a = std::pow(1.0 / a_, 1.0 / n1_);
        const double r_b = std::pow(1.0 / b_, 1.0 / n1_);
        extent_ = std::max(r_a, r_b);
        if (!std::isfinite(extent_) || extent_ <= 0.0) extent_ = 1.0;
    }

    Vec2 evaluate(double t) const override {
        const double phi = t * t_max_;
        const double mp = m_ * phi / 4.0;
        const double term1 = std::pow(std::abs(std::cos(mp) / a_), n2_);
        const double term2 = std::pow(std::abs(std::sin(mp) / b_), n3_);
        const double sum = term1 + term2;
        if (sum <= 0.0 || !std::isfinite(sum)) return {0.0, 0.0};
        const double r = std::pow(sum, -1.0 / n1_);
        return {r * std::cos(phi), r * std::sin(phi)};
    }

    std::pair<Vec2, Vec2> bounding_box() const override {
        return {{-extent_, -extent_}, {extent_, extent_}};
    }

private:
    double m_;
    double n1_;
    double n2_;
    double n3_;
    double a_;
    double b_;
    double t_max_;
    double extent_;
};

}  // namespace caustic
