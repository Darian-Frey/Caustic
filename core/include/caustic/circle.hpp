#pragma once

#include <cmath>
#include <numbers>
#include <utility>

#include <caustic/curve.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

class Circle : public ParametricCurve {
public:
    Vec2 evaluate(double t) const override {
        const double angle = 2.0 * std::numbers::pi * t;
        return {std::cos(angle), std::sin(angle)};
    }

    std::pair<Vec2, Vec2> bounding_box() const override {
        return {{-1.0, -1.0}, {1.0, 1.0}};
    }
};

}  // namespace caustic
