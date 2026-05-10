#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <string>
#include <vector>

#include <caustic/preset.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

// Array tools — free functions that take a source Layer and emit N derived
// layers. The caller uses these to assemble symmetric / tiled / mirrored
// scenes from one motif (e.g. a single LinearEnvelope rotated 6× around
// the centre reproduces the radial-bowtie patterns from the reference
// images). Each derived layer is independently editable afterward; there's
// no live link back to the source.

// Rotate a vector by angle (radians) around origin.
inline Vec2 rotate_origin(Vec2 v, double angle) {
    const double c = std::cos(angle);
    const double s = std::sin(angle);
    return {v.x * c - v.y * s, v.x * s + v.y * c};
}

// Rotational array: N copies of the source layer, each rotated by
// i · (2π / N) around `center`. The i-th copy's transform.translate moves
// so the layer's original anchor point rotates around `center`, while its
// transform.rotate_rad gets the same angle so the geometry itself rotates
// with the position.
inline std::vector<Layer> rotational_array(const Layer& source, int n, Vec2 center = {0.0, 0.0}) {
    std::vector<Layer> out;
    if (n <= 0) return out;
    out.reserve(static_cast<std::size_t>(n));
    const double step = 2.0 * std::numbers::pi / static_cast<double>(n);
    for (int i = 0; i < n; ++i) {
        const double angle = step * static_cast<double>(i);
        Layer copy = source;
        // Position rotates around `center`:
        //   new_translate = center + R(angle) · (source.translate - center)
        const Vec2 rel = {source.transform.translate.x - center.x,
                          source.transform.translate.y - center.y};
        const Vec2 rotated_rel = rotate_origin(rel, angle);
        copy.transform.translate = {center.x + rotated_rel.x, center.y + rotated_rel.y};
        // Geometry orientation composes:
        copy.transform.rotate_rad = source.transform.rotate_rad + angle;
        copy.name = source.name + " #" + std::to_string(i);
        out.push_back(std::move(copy));
    }
    return out;
}

// Grid tile: rows × cols copies, each translated by (col · spacing.x,
// row · spacing.y) relative to the source's translate. The grid is centred
// on the source's translate when rows × cols is symmetric.
inline std::vector<Layer> grid_tile(const Layer& source, int rows, int cols, Vec2 spacing) {
    std::vector<Layer> out;
    if (rows <= 0 || cols <= 0) return out;
    out.reserve(static_cast<std::size_t>(rows * cols));
    const double row_offset = (rows - 1) * 0.5;
    const double col_offset = (cols - 1) * 0.5;
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < cols; ++col) {
            Layer copy = source;
            const double dx = (static_cast<double>(col) - col_offset) * spacing.x;
            const double dy = (static_cast<double>(row) - row_offset) * spacing.y;
            copy.transform.translate.x = source.transform.translate.x + dx;
            copy.transform.translate.y = source.transform.translate.y + dy;
            copy.name = source.name + " [" + std::to_string(row) + "," + std::to_string(col) + "]";
            out.push_back(std::move(copy));
        }
    }
    return out;
}

enum class MirrorAxis { X, Y };

// Mirror reflect: source plus one mirrored copy. axis=X reflects across the
// x-axis (negates y); axis=Y reflects across the y-axis (negates x).
inline std::vector<Layer> mirror_reflect(const Layer& source, MirrorAxis axis) {
    std::vector<Layer> out;
    out.reserve(2);
    out.push_back(source);
    Layer mirrored = source;
    if (axis == MirrorAxis::Y) {
        mirrored.transform.mirror_x = !mirrored.transform.mirror_x;
        mirrored.transform.translate.x = -mirrored.transform.translate.x;
    } else {
        mirrored.transform.mirror_y = !mirrored.transform.mirror_y;
        mirrored.transform.translate.y = -mirrored.transform.translate.y;
    }
    mirrored.name = source.name + " (mirror)";
    out.push_back(std::move(mirrored));
    return out;
}

}  // namespace caustic
