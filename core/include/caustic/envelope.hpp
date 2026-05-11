#pragma once

#include <cmath>
#include <numbers>
#include <variant>

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

using Envelope = std::variant<Static, Linear, Sine>;

inline double evaluate(const Envelope& env, double t) {
    if (const auto* s = std::get_if<Static>(&env)) return s->value;
    if (const auto* l = std::get_if<Linear>(&env)) return l->v0 + (l->v1 - l->v0) * t;
    if (const auto* w = std::get_if<Sine>(&env)) {
        const double arg = w->frequency * t * 2.0 * std::numbers::pi + w->phase;
        return w->offset + w->amplitude * std::sin(arg);
    }
    return 0.0;
}

}  // namespace caustic::anim
