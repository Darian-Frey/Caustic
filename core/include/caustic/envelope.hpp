#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <variant>
#include <vector>

namespace caustic::anim {

// Time-varying value evaluators. Each envelope maps normalised
// t ∈ [0, 1] (one pass through the animation's duration) to a parameter
// value. The variant lets a single AnimationSpec hold any of the
// envelope types without runtime polymorphism.

struct Static {
    double value = 0.0;
};

struct Linear {
    double v0 = 0.0;
    double v1 = 1.0;
};

struct Sine {
    double amplitude = 1.0;
    double frequency = 1.0;  // cycles over the animation duration
    double phase     = 0.0;
    double offset    = 0.0;
};

// Piecewise-linear envelope through arbitrary control points. Keys are
// (t, value) pairs; evaluation finds the bracket containing t and lerps
// between adjacent keys. Out-of-range t clamps to the first / last key's
// value. Keys are expected to be sorted by t — the editor enforces this.
struct Keyframed {
    std::vector<std::pair<double, double>> keys = {{0.0, 0.0}, {1.0, 1.0}};
};

using Envelope = std::variant<Static, Linear, Sine, Keyframed>;

inline double evaluate(const Envelope& env, double t) {
    if (const auto* s = std::get_if<Static>(&env)) return s->value;
    if (const auto* l = std::get_if<Linear>(&env)) return l->v0 + (l->v1 - l->v0) * t;
    if (const auto* w = std::get_if<Sine>(&env)) {
        const double arg = w->frequency * t * 2.0 * std::numbers::pi + w->phase;
        return w->offset + w->amplitude * std::sin(arg);
    }
    if (const auto* k = std::get_if<Keyframed>(&env)) {
        const auto& keys = k->keys;
        if (keys.empty()) return 0.0;
        if (keys.size() == 1) return keys.front().second;
        if (t <= keys.front().first) return keys.front().second;
        if (t >= keys.back().first)  return keys.back().second;
        // Find the bracket containing t and lerp.
        for (std::size_t i = 1; i < keys.size(); ++i) {
            if (t <= keys[i].first) {
                const double t0 = keys[i - 1].first;
                const double v0 = keys[i - 1].second;
                const double t1 = keys[i].first;
                const double v1 = keys[i].second;
                const double span = t1 - t0;
                if (span <= 0.0) return v0;
                const double a = (t - t0) / span;
                return v0 + (v1 - v0) * a;
            }
        }
        return keys.back().second;
    }
    return 0.0;
}

}  // namespace caustic::anim
