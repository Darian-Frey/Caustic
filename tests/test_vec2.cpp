#include <doctest/doctest.h>

#include <caustic/vec2.hpp>

using caustic::Vec2;

TEST_CASE("Vec2 default-constructs to zero") {
    Vec2 v;
    CHECK(v.x == 0.0);
    CHECK(v.y == 0.0);
}

TEST_CASE("Vec2 value-constructs from doubles") {
    Vec2 v(3.0, 4.0);
    CHECK(v.x == 3.0);
    CHECK(v.y == 4.0);
}

TEST_CASE("Vec2 equality compares componentwise") {
    CHECK(Vec2(1.0, 2.0) == Vec2(1.0, 2.0));
    CHECK_FALSE(Vec2(1.0, 2.0) == Vec2(1.0, 3.0));
}

TEST_CASE("Vec2 addition is componentwise") {
    CHECK(Vec2(1.0, 2.0) + Vec2(3.0, 4.0) == Vec2(4.0, 6.0));
}

TEST_CASE("Vec2 subtraction is componentwise") {
    CHECK(Vec2(5.0, 6.0) - Vec2(1.0, 2.0) == Vec2(4.0, 4.0));
}

TEST_CASE("Vec2 scalar multiply commutes") {
    CHECK(Vec2(1.0, 2.0) * 3.0 == Vec2(3.0, 6.0));
    CHECK(3.0 * Vec2(1.0, 2.0) == Vec2(3.0, 6.0));
}
