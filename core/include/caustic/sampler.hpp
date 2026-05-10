#pragma once

#include <vector>

#include <caustic/curve.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Sample a parametric curve at `samples` evenly-spaced t values in [0, 1].
inline std::vector<Vec2> sample_curve(const ParametricCurve& curve, int samples) {
    std::vector<Vec2> polyline;
    if (samples < 2) return polyline;
    polyline.reserve(static_cast<std::size_t>(samples));
    const double denom = static_cast<double>(samples - 1);
    for (int i = 0; i < samples; ++i) {
        polyline.push_back(curve.evaluate(static_cast<double>(i) / denom));
    }
    return polyline;
}

}  // namespace caustic
