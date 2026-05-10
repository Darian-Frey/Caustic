#pragma once

#include <utility>

#include <caustic/vec2.hpp>

namespace caustic {

class ParametricCurve {
public:
    virtual ~ParametricCurve() = default;

    // Evaluate at parameter t ∈ [0, 1].
    virtual Vec2 evaluate(double t) const = 0;

    // (min, max) corner pair in math-up world coordinates.
    virtual std::pair<Vec2, Vec2> bounding_box() const = 0;
};

}  // namespace caustic
