#pragma once

namespace caustic {

struct Vec2 {
    double x;
    double y;

    constexpr Vec2() : x(0.0), y(0.0) {}
    constexpr Vec2(double x_, double y_) : x(x_), y(y_) {}

    constexpr bool operator==(Vec2 rhs) const {
        return x == rhs.x && y == rhs.y;
    }
};

}  // namespace caustic
