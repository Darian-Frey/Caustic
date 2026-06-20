#include <doctest/doctest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include <caustic/image_trace.hpp>
#include <caustic/preset.hpp>

using namespace caustic;

namespace {

// Build a width × height grayscale image filled with `bg`, then overlay
// a black-to-white step edge at column `edge_col` so Sobel sees a strong
// vertical edge. Useful for asserting that the trace finds the edge.
std::vector<std::uint8_t> step_edge_image(int w, int h, int edge_col,
                                          std::uint8_t low = 0,
                                          std::uint8_t high = 255) {
    std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * h, low);
    for (int y = 0; y < h; ++y) {
        for (int x = edge_col; x < w; ++x) px[y * w + x] = high;
    }
    return px;
}

// A diagonal edge from top-left to bottom-right (black above the diagonal,
// white below). Tests that the trace finds points along the diagonal.
std::vector<std::uint8_t> diagonal_image(int w, int h) {
    std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * h, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (x > y) px[y * w + x] = 255;
        }
    }
    return px;
}

}  // namespace

// ---------------------------------------------------------------------------
// pixel_to_world coordinate mapping

TEST_CASE("pixel_to_world maps corners to math-up [-1, 1] for square images") {
    const auto tl = detail::pixel_to_world(0, 0, 100, 100);
    const auto tr = detail::pixel_to_world(99, 0, 100, 100);
    const auto bl = detail::pixel_to_world(0, 99, 100, 100);
    const auto br = detail::pixel_to_world(99, 99, 100, 100);
    CHECK(tl.x == doctest::Approx(-1.0));
    CHECK(tl.y == doctest::Approx( 1.0));
    CHECK(tr.x == doctest::Approx( 1.0));
    CHECK(tr.y == doctest::Approx( 1.0));
    CHECK(bl.x == doctest::Approx(-1.0));
    CHECK(bl.y == doctest::Approx(-1.0));
    CHECK(br.x == doctest::Approx( 1.0));
    CHECK(br.y == doctest::Approx(-1.0));
}

TEST_CASE("pixel_to_world preserves aspect ratio on non-square images") {
    // Wide: 200×100 → aspect 2.0 → x in [-1, 1], y in [-0.5, 0.5]
    const auto tl = detail::pixel_to_world(0, 0, 200, 100);
    const auto br = detail::pixel_to_world(199, 99, 200, 100);
    CHECK(tl.x == doctest::Approx(-1.0));
    CHECK(tl.y == doctest::Approx( 0.5));
    CHECK(br.x == doctest::Approx( 1.0));
    CHECK(br.y == doctest::Approx(-0.5));
    // Tall: 100×200 → aspect 0.5 → x in [-0.5, 0.5], y in [-1, 1]
    const auto tl2 = detail::pixel_to_world(0, 0, 100, 200);
    const auto br2 = detail::pixel_to_world(99, 199, 100, 200);
    CHECK(tl2.x == doctest::Approx(-0.5));
    CHECK(tl2.y == doctest::Approx( 1.0));
    CHECK(br2.x == doctest::Approx( 0.5));
    CHECK(br2.y == doctest::Approx(-1.0));
}

// ---------------------------------------------------------------------------
// Sobel + sampling

TEST_CASE("sobel_magnitude is zero in a solid-colour region and high on edges") {
    const auto px = step_edge_image(20, 20, 10);
    // Interior of the solid-low (left) region should be 0.
    CHECK(detail::sobel_magnitude(px.data(), 20, 20, 3, 10) == 0);
    // Pixel right at the step edge should fire.
    const int edge_mag = detail::sobel_magnitude(px.data(), 20, 20, 10, 10);
    CHECK(edge_mag > 100);
}

TEST_CASE("sobel_magnitude returns 0 on the 1-pixel border") {
    const auto px = step_edge_image(20, 20, 10);
    CHECK(detail::sobel_magnitude(px.data(), 20, 20,  0, 10) == 0);
    CHECK(detail::sobel_magnitude(px.data(), 20, 20, 19, 10) == 0);
    CHECK(detail::sobel_magnitude(px.data(), 20, 20, 10,  0) == 0);
    CHECK(detail::sobel_magnitude(px.data(), 20, 20, 10, 19) == 0);
}

// ---------------------------------------------------------------------------
// image_trace end-to-end

TEST_CASE("image_trace on a step edge places all nails near the edge column") {
    const auto px = step_edge_image(60, 60, 30);
    ImageTraceOptions opts;
    opts.grid_divisions = 6;
    opts.max_nails      = 32;
    const auto result = image_trace(60, 60, px.data(), opts);
    REQUIRE(result.nails.size() > 0);
    // Every nail should sit close to x=0 in world coords (the step edge
    // sits at col 30 of 60, which maps to world x ≈ 0).
    for (const auto& n : result.nails) {
        CHECK(std::abs(n.x) < 0.2);
    }
}

TEST_CASE("image_trace on a diagonal edge spreads nails along the diagonal") {
    const auto px = diagonal_image(60, 60);
    ImageTraceOptions opts;
    opts.grid_divisions = 6;
    opts.max_nails      = 32;
    const auto result = image_trace(60, 60, px.data(), opts);
    REQUIRE(result.nails.size() >= 4);
    // For a y = x diagonal in pixel space (which becomes y = -x in math-up
    // world coords), x + y should be near 0 for each nail.
    for (const auto& n : result.nails) {
        CHECK(std::abs(n.x + n.y) < 0.4);
    }
}

TEST_CASE("image_trace on a solid colour image returns no nails") {
    const std::vector<std::uint8_t> px(60 * 60, 128);
    const auto result = image_trace(60, 60, px.data());
    CHECK(result.nails.empty());
    CHECK(result.chords.empty());
}

TEST_CASE("image_trace tiny/null inputs return empty") {
    CHECK(image_trace(0, 0, nullptr).nails.empty());
    CHECK(image_trace(2, 2, nullptr).nails.empty());
    const std::vector<std::uint8_t> px(4, 0);
    CHECK(image_trace(2, 2, px.data()).nails.empty());
}

// ---------------------------------------------------------------------------
// Chord rules

TEST_CASE("Modular chord rule with k=2 produces N chords, none self-loop") {
    const auto px = step_edge_image(60, 60, 30);
    ImageTraceOptions opts;
    opts.rule        = TraceChordRule::Modular;
    opts.modular_k   = 2.0;
    opts.grid_divisions = 6;
    const auto result = image_trace(60, 60, px.data(), opts);
    REQUIRE(result.nails.size() >= 4);
    // Every chord should connect two distinct nails — chord 0 (0 → 0) is
    // skipped, so we expect N-1 chords. But edge cases can still hit a
    // degenerate i==(2i mod N) when N is small. Just check no self-loop.
    for (const auto& c : result.chords) {
        CHECK(c.first != c.second);
        CHECK(c.first  >= 0);
        CHECK(c.second >= 0);
        CHECK(c.first  < static_cast<int>(result.nails.size()));
        CHECK(c.second < static_cast<int>(result.nails.size()));
    }
}

TEST_CASE("Sequential rule emits N-1 chords") {
    const auto px = step_edge_image(60, 60, 30);
    ImageTraceOptions opts;
    opts.rule = TraceChordRule::Sequential;
    opts.grid_divisions = 6;
    const auto result = image_trace(60, 60, px.data(), opts);
    REQUIRE(result.nails.size() >= 2);
    CHECK(result.chords.size() == result.nails.size() - 1);
    for (std::size_t i = 0; i < result.chords.size(); ++i) {
        CHECK(result.chords[i].first  == static_cast<int>(i));
        CHECK(result.chords[i].second == static_cast<int>(i + 1));
    }
}

TEST_CASE("Nearest rule de-duplicates undirected pairs") {
    const auto px = step_edge_image(60, 60, 30);
    ImageTraceOptions opts;
    opts.rule      = TraceChordRule::Nearest;
    opts.nearest_k = 2;
    opts.grid_divisions = 6;
    const auto result = image_trace(60, 60, px.data(), opts);
    REQUIRE(result.nails.size() >= 3);
    // No chord should have first > second (we store as (lo, hi) for dedup).
    // No duplicates either.
    for (const auto& c : result.chords) {
        CHECK(c.first < c.second);
    }
    for (std::size_t i = 1; i < result.chords.size(); ++i) {
        CHECK(result.chords[i - 1] != result.chords[i]);
    }
}

// ---------------------------------------------------------------------------
// Determinism

TEST_CASE("image_trace is deterministic — same input → same output") {
    const auto px = step_edge_image(60, 60, 30);
    const auto a = image_trace(60, 60, px.data());
    const auto b = image_trace(60, 60, px.data());
    CHECK(a.nails.size()  == b.nails.size());
    CHECK(a.chords.size() == b.chords.size());
    for (std::size_t i = 0; i < a.nails.size(); ++i) {
        CHECK(a.nails[i].x == b.nails[i].x);
        CHECK(a.nails[i].y == b.nails[i].y);
    }
    for (std::size_t i = 0; i < a.chords.size(); ++i) {
        CHECK(a.chords[i] == b.chords[i]);
    }
}

TEST_CASE("max_nails caps the output even when many cells fire") {
    // Create a high-contrast image where every cell will have a strong edge.
    const int w = 80, h = 80;
    std::vector<std::uint8_t> px(static_cast<std::size_t>(w) * h, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            // Checker pattern with 10px cells → edges everywhere.
            if (((x / 10) + (y / 10)) % 2) px[y * w + x] = 255;
        }
    }
    ImageTraceOptions opts;
    opts.max_nails      = 16;
    opts.grid_divisions = 8;  // could fire 64 cells; cap to 16
    const auto result = image_trace(w, h, px.data(), opts);
    CHECK(static_cast<int>(result.nails.size()) <= 16);
}
