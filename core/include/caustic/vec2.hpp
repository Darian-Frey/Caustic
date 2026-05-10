#pragma once

namespace caustic {

struct Vec2 {
    double x;
    double y;

    constexpr Vec2() : x(0.0), y(0.0) {}
    constexpr Vec2(double x_, double y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(Vec2 rhs) const { return {x + rhs.x, y + rhs.y}; }
    constexpr Vec2 operator-(Vec2 rhs) const { return {x - rhs.x, y - rhs.y}; }
    constexpr Vec2 operator*(double s) const { return {x * s, y * s}; }

    constexpr bool operator==(Vec2 rhs) const {
        return x == rhs.x && y == rhs.y;
    }
};

constexpr Vec2 operator*(double s, Vec2 v) { return v * s; }

}  // namespace caustic
