#pragma once

#include <caustic/color.hpp>

namespace caustic {

class ColorMap {
public:
    virtual ~ColorMap() = default;

    // Map t ∈ [0, 1] to an RGBA color.
    virtual Color at(double t) const = 0;
};

class Solid : public ColorMap {
public:
    explicit Solid(Color c) : color_(c) {}
    Color at(double) const override { return color_; }

private:
    Color color_;
};

class LinearGradient : public ColorMap {
public:
    LinearGradient(Color start, Color end) : start_(start), end_(end) {}
    Color at(double t) const override { return lerp(start_, end_, t); }

private:
    Color start_;
    Color end_;
};

class HsvSweep : public ColorMap {
public:
    HsvSweep(double hue_start, double hue_end, double saturation, double value)
        : h0_(hue_start), h1_(hue_end), s_(saturation), v_(value) {}

    Color at(double t) const override {
        const double h = h0_ + (h1_ - h0_) * t;
        return hsv_to_rgb(h, s_, v_);
    }

private:
    double h0_;
    double h1_;
    double s_;
    double v_;
};

class Diverging : public ColorMap {
public:
    Diverging(Color negative, Color midpoint, Color positive)
        : neg_(negative), mid_(midpoint), pos_(positive) {}

    Color at(double t) const override {
        if (t <= 0.5) {
            return lerp(neg_, mid_, t * 2.0);
        }
        return lerp(mid_, pos_, (t - 0.5) * 2.0);
    }

private:
    Color neg_;
    Color mid_;
    Color pos_;
};

}  // namespace caustic
