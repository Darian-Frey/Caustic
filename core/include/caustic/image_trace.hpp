#pragma once

// Image trace → CustomChord layer. Takes a width × height grayscale image
// buffer (top-left origin, row-major, one byte per pixel) and emits a
// `CustomChordParams` whose nails sit on the strongest edges of the image
// and whose chord pairs follow one of three rules. The output is a normal
// CustomChord layer the user can keep editing — this is the angle that
// distinguishes Caustic from photo-string-art tools, whose output is a
// peg-board build script.
//
// Header-only, no raylib / file-format dependency. The app does the image-
// loading and grayscale conversion via raylib; the core takes the resulting
// pixel buffer and runs the trace. Tests can drive it with synthetic
// buffers built by hand (no PNG decoder needed in the test path).
//
// Pipeline:
//   1. Sobel gradient magnitude (|Gx| + |Gy|, clamped to 0..255).
//   2. Stratified sampling: divide image into N×N cells, take the strongest
//      pixel in each cell IF above edge_threshold.
//   3. Clamp the result to max_nails (sorted by edge strength).
//   4. Map pixel coords to math-up world coords in [-1, 1] preserving
//      aspect ratio.
//   5. Generate chord pairs by the chosen rule.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <caustic/preset.hpp>

namespace caustic {

enum class TraceChordRule {
    // Classic string-art rule: chord i connects nail i to nail (round(k·i) mod N).
    Modular,
    // Sequential trace: chord i connects nail i to nail i+1.
    Sequential,
    // Each nail connects to its `nearest_k` nearest neighbours (pairs
    // de-duplicated so an undirected edge is emitted once).
    Nearest,
};

struct ImageTraceOptions {
    // Hard cap on emitted nails. The stratified pass may produce fewer.
    int max_nails = 100;
    // Grid resolution for stratified sampling. N×N cells; one nail per cell
    // (or zero if no pixel in the cell crosses the edge threshold). Keep
    // grid_divisions ≈ sqrt(max_nails) to avoid most cells being culled.
    int grid_divisions = 12;
    // Sobel magnitude threshold (0..255). Pixels below are ignored.
    int edge_threshold = 60;
    TraceChordRule rule = TraceChordRule::Modular;
    // Modular rule multiplier — chord i → (round(k·i)) mod N.
    double modular_k = 2.0;
    // Nearest rule — how many nearest neighbours each nail connects to.
    int nearest_k = 2;
};

namespace detail {

// Sobel magnitude of one pixel. Returns 0 at the 1-pixel border (where the
// kernel can't fit) so callers can ignore borders by checking the cell range.
inline int sobel_magnitude(const std::uint8_t* px, int w, int h, int x, int y) {
    if (x <= 0 || y <= 0 || x >= w - 1 || y >= h - 1) return 0;
    const std::uint8_t* row_up = px + (y - 1) * w;
    const std::uint8_t* row    = px +  y      * w;
    const std::uint8_t* row_dn = px + (y + 1) * w;
    const int gx =
        - row_up[x - 1] +     row_up[x + 1]
        - 2 * row[x - 1]  + 2 * row[x + 1]
        - row_dn[x - 1] +     row_dn[x + 1];
    const int gy =
        - row_up[x - 1] - 2 * row_up[x] - row_up[x + 1]
        + row_dn[x - 1] + 2 * row_dn[x] + row_dn[x + 1];
    const int mag = std::abs(gx) + std::abs(gy);
    return std::min(255, mag);
}

struct EdgePoint {
    int px, py;     // pixel coords (top-left origin)
    int magnitude;  // Sobel magnitude 0..255
};

// Stratified pass: one strongest pixel per grid cell above threshold.
inline std::vector<EdgePoint> sample_edges(
    int w, int h, const std::uint8_t* pixels,
    int grid_divisions, int edge_threshold) {
    if (w < 3 || h < 3 || grid_divisions < 1) return {};

    std::vector<EdgePoint> hits;
    hits.reserve(grid_divisions * grid_divisions);

    const int cell_w = std::max(1, w / grid_divisions);
    const int cell_h = std::max(1, h / grid_divisions);

    for (int gy = 0; gy < grid_divisions; ++gy) {
        for (int gx = 0; gx < grid_divisions; ++gx) {
            const int x0 = gx * cell_w;
            const int y0 = gy * cell_h;
            const int x1 = (gx == grid_divisions - 1) ? w : x0 + cell_w;
            const int y1 = (gy == grid_divisions - 1) ? h : y0 + cell_h;

            EdgePoint best{-1, -1, 0};
            for (int py = y0; py < y1; ++py) {
                for (int px = x0; px < x1; ++px) {
                    const int m = sobel_magnitude(pixels, w, h, px, py);
                    if (m > best.magnitude) best = {px, py, m};
                }
            }
            if (best.magnitude >= edge_threshold) hits.push_back(best);
        }
    }
    return hits;
}

// Map (px, py) — top-left origin, integer pixels — to math-up world coords in
// [-1, 1] preserving aspect ratio. Wider images fill the x extent; taller
// images fill the y extent.
inline Vec2 pixel_to_world(int px, int py, int w, int h) {
    const double aspect = (h <= 0) ? 1.0 : static_cast<double>(w) / h;
    const double extent_x = (aspect >= 1.0) ? 1.0 : aspect;
    const double extent_y = (aspect >= 1.0) ? 1.0 / aspect : 1.0;
    const double u = (w > 1) ? static_cast<double>(px) / (w - 1) : 0.5;
    const double v = (h > 1) ? static_cast<double>(py) / (h - 1) : 0.5;
    return {
        -extent_x + 2.0 * extent_x * u,
         extent_y - 2.0 * extent_y * v,  // flip Y for math-up
    };
}

// Chord generation per rule. N = nails.size(). Output is the chord list.
inline std::vector<std::pair<int, int>> build_chords(
    int n, TraceChordRule rule, double modular_k, int nearest_k,
    const std::vector<Vec2>& nails) {
    std::vector<std::pair<int, int>> chords;
    if (n < 2) return chords;
    switch (rule) {
        case TraceChordRule::Modular: {
            chords.reserve(n);
            for (int i = 0; i < n; ++i) {
                int j = static_cast<int>(std::lround(modular_k * i)) % n;
                if (j < 0) j += n;
                if (j != i) chords.emplace_back(i, j);
            }
            break;
        }
        case TraceChordRule::Sequential: {
            chords.reserve(n - 1);
            for (int i = 0; i + 1 < n; ++i) chords.emplace_back(i, i + 1);
            break;
        }
        case TraceChordRule::Nearest: {
            const int k = std::clamp(nearest_k, 1, n - 1);
            chords.reserve(n * k);
            std::vector<std::pair<double, int>> dists;
            dists.reserve(n - 1);
            for (int i = 0; i < n; ++i) {
                dists.clear();
                for (int j = 0; j < n; ++j) {
                    if (j == i) continue;
                    const double dx = nails[j].x - nails[i].x;
                    const double dy = nails[j].y - nails[i].y;
                    dists.emplace_back(dx * dx + dy * dy, j);
                }
                std::partial_sort(dists.begin(), dists.begin() + k, dists.end());
                for (int t = 0; t < k; ++t) {
                    const int j = dists[t].second;
                    // De-dupe undirected edges by always emitting (lo, hi).
                    const int lo = std::min(i, j);
                    const int hi = std::max(i, j);
                    chords.emplace_back(lo, hi);
                }
            }
            std::sort(chords.begin(), chords.end());
            chords.erase(std::unique(chords.begin(), chords.end()), chords.end());
            break;
        }
    }
    return chords;
}

}  // namespace detail

// Trace an image to a CustomChord layer. Returns empty `nails`/`chords`
// when no pixel meets the edge threshold or the input is too small.
inline CustomChordParams image_trace(int width, int height,
                                     const std::uint8_t* pixels,
                                     const ImageTraceOptions& opts = {}) {
    CustomChordParams out;
    if (!pixels || width < 3 || height < 3) return out;

    auto hits = detail::sample_edges(width, height, pixels,
                                     std::max(1, opts.grid_divisions),
                                     std::clamp(opts.edge_threshold, 0, 255));
    if (hits.empty()) return out;

    // If the stratified pass produced more cells than max_nails, keep the
    // strongest. (Otherwise we already only have one per cell.)
    const int cap = std::max(2, opts.max_nails);
    if (static_cast<int>(hits.size()) > cap) {
        std::partial_sort(hits.begin(), hits.begin() + cap, hits.end(),
                          [](const detail::EdgePoint& a, const detail::EdgePoint& b) {
                              return a.magnitude > b.magnitude;
                          });
        hits.resize(cap);
    }

    // Stable sort by scan-line order so chord rules produce predictable
    // patterns (modular k=2 traces top-to-bottom, sequential snakes through
    // rows). Top-to-bottom then left-to-right matches reading order.
    std::sort(hits.begin(), hits.end(),
              [](const detail::EdgePoint& a, const detail::EdgePoint& b) {
                  if (a.py != b.py) return a.py < b.py;
                  return a.px < b.px;
              });

    out.nails.reserve(hits.size());
    for (const auto& h : hits) {
        out.nails.push_back(detail::pixel_to_world(h.px, h.py, width, height));
    }
    out.chords = detail::build_chords(static_cast<int>(out.nails.size()),
                                      opts.rule, opts.modular_k, opts.nearest_k,
                                      out.nails);
    return out;
}

}  // namespace caustic
