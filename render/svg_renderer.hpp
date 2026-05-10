#pragma once

#include <filesystem>
#include <string>

#include <caustic/geometry_buffer.hpp>
#include <caustic/style.hpp>

namespace caustic {

struct SvgOptions {
    // Canvas dimensions in SVG units (viewBox is "0 0 width height").
    double width = 1024.0;
    double height = 1024.0;

    // Margin around the geometry, as a fraction of min(width, height).
    double margin = 0.05;

    // Plotter mode: omit background, strip opacity, single stroke colour,
    // chord set sorted by start point (nearest-neighbour pen-travel approximation).
    // Polylines are emitted as single <polyline> elements (one pen-down per curve).
    bool plotter_mode = false;
    std::string plotter_color = "#000000";

    // Douglas–Peucker simplification tolerance. 0 = off. Reserved for a future
    // optional pass; currently not applied.
    double simplify_epsilon = 0.0;
};

// Render the buffer + style to an SVG string. Same input → byte-identical output.
std::string render_svg(const GeometryBuffer& geo, const Style& style, const SvgOptions& opts = {});

// Convenience: render + write to disk. Creates parent directories.
void write_svg(const std::filesystem::path& path,
               const GeometryBuffer& geo,
               const Style& style,
               const SvgOptions& opts = {});

}  // namespace caustic
