#include "svg_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <caustic/color.hpp>
#include <caustic/indexer.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

namespace {

// SPEC §4.3 determinism: fixed 6-decimal precision for every coordinate and
// stroke value. Identical input → identical output bytes.
std::string fmt_f(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", v);
    return buf;
}

std::string color_to_hex(Color c) {
    auto byte = [](double v) -> int {
        return static_cast<int>(std::clamp(v * 255.0 + 0.5, 0.0, 255.0));
    };
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%02x%02x%02x", byte(c.r), byte(c.g), byte(c.b));
    return buf;
}

struct Bounds { double xmin = 0, ymin = 0, xmax = 0, ymax = 0; bool empty = true; };

Bounds compute_bounds(const std::vector<LayerRender>& layers) {
    Bounds b;
    auto include = [&](Vec2 v) {
        if (b.empty) {
            b.xmin = b.xmax = v.x;
            b.ymin = b.ymax = v.y;
            b.empty = false;
        } else {
            b.xmin = std::min(b.xmin, v.x);
            b.xmax = std::max(b.xmax, v.x);
            b.ymin = std::min(b.ymin, v.y);
            b.ymax = std::max(b.ymax, v.y);
        }
    };
    for (const auto& layer : layers) {
        for (const auto& c : layer.geometry.chords) { include(c.a); include(c.b); }
        for (const auto& p : layer.geometry.polylines) for (auto v : p) include(v);
        for (const auto& pt : layer.geometry.points) include(pt);
    }
    if (b.empty) { b.xmin = b.ymin = -1.0; b.xmax = b.ymax = 1.0; b.empty = false; }
    return b;
}

double segment_length(Vec2 a, Vec2 b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

double max_chord_length(const ChordSet& chords) {
    double m = 0.0;
    for (const auto& c : chords) m = std::max(m, segment_length(c.a, c.b));
    return m;
}

double max_polyline_segment_length(const std::vector<Vec2>& p) {
    double m = 0.0;
    for (std::size_t i = 1; i < p.size(); ++i) m = std::max(m, segment_length(p[i - 1], p[i]));
    return m;
}

double lerp_width(double w_min, double w_max, double t) {
    if (t < 0.0) t = 0.0;
    else if (t > 1.0) t = 1.0;
    return w_min + (w_max - w_min) * t;
}

double remap_cyclic(double t, bool cyclic) {
    return cyclic ? (1.0 - std::abs(2.0 * t - 1.0)) : t;
}

struct Mapping {
    double scale;
    double cx_canvas, cy_canvas;
    double cx_world,  cy_world;
};

Mapping make_mapping(const Bounds& b, const SvgOptions& opts) {
    const double bbox_w = std::max(1e-9, b.xmax - b.xmin);
    const double bbox_h = std::max(1e-9, b.ymax - b.ymin);
    const double margin_px = opts.margin * std::min(opts.width, opts.height);
    const double avail_w = std::max(1.0, opts.width  - 2.0 * margin_px);
    const double avail_h = std::max(1.0, opts.height - 2.0 * margin_px);
    Mapping m;
    m.scale = std::min(avail_w / bbox_w, avail_h / bbox_h);
    m.cx_canvas = opts.width / 2.0;
    m.cy_canvas = opts.height / 2.0;
    m.cx_world = (b.xmin + b.xmax) / 2.0;
    m.cy_world = (b.ymin + b.ymax) / 2.0;
    return m;
}

Vec2 to_svg(const Mapping& m, Vec2 v) {
    return {
        m.cx_canvas + (v.x - m.cx_world) * m.scale,
        m.cy_canvas - (v.y - m.cy_world) * m.scale,
    };
}

void emit_line(std::ostringstream& out, Vec2 a, Vec2 b,
               const std::string& stroke_hex, double stroke_width,
               double stroke_opacity, bool include_opacity) {
    out << "    <line x1=\"" << fmt_f(a.x) << "\" y1=\"" << fmt_f(a.y)
        << "\" x2=\"" << fmt_f(b.x) << "\" y2=\"" << fmt_f(b.y)
        << "\" stroke=\"" << stroke_hex
        << "\" stroke-width=\"" << fmt_f(stroke_width) << "\"";
    if (include_opacity) {
        out << " stroke-opacity=\"" << fmt_f(stroke_opacity) << "\"";
    }
    out << "/>\n";
}

void emit_polyline(std::ostringstream& out, const std::vector<Vec2>& pts,
                   const Mapping& m,
                   const std::string& stroke_hex, double stroke_width,
                   double stroke_opacity, bool include_opacity) {
    out << "    <polyline fill=\"none\" stroke=\"" << stroke_hex
        << "\" stroke-width=\"" << fmt_f(stroke_width) << "\"";
    if (include_opacity) {
        out << " stroke-opacity=\"" << fmt_f(stroke_opacity) << "\"";
    }
    out << " points=\"";
    for (std::size_t i = 0; i < pts.size(); ++i) {
        const Vec2 s = to_svg(m, pts[i]);
        if (i) out << ' ';
        out << fmt_f(s.x) << ',' << fmt_f(s.y);
    }
    out << "\"/>\n";
}

void render_layer(std::ostringstream& out, std::size_t idx, const LayerRender& L,
                  const Mapping& m, const SvgOptions& opts, bool with_opacity) {
    const Style& style = L.style;
    const GeometryBuffer& geo = L.geometry;
    if (!style.color_map) return;

    const std::string layer_label = L.name.empty() ? ("layer-" + std::to_string(idx)) : L.name;
    out << "  <g id=\"layer-" << idx << "\" inkscape:label=\"" << layer_label << "\">\n";

    // Chords --------------------------------------------------------------
    if (!geo.chords.empty()) {
        std::vector<std::size_t> order(geo.chords.size());
        for (std::size_t i = 0; i < order.size(); ++i) order[i] = i;
        if (opts.plotter_mode) {
            std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
                const Vec2& pa = geo.chords[a].a;
                const Vec2& pb = geo.chords[b].a;
                if (pa.x != pb.x) return pa.x < pb.x;
                return pa.y < pb.y;
            });
        }

        const std::size_t N = geo.chords.size();
        const double max_len = max_chord_length(geo.chords);
        const bool have_overrides = geo.chord_color_overrides.size() == N;
        const bool have_gradient  = geo.chord_end_color_overrides.size() == N;

        for (std::size_t i : order) {
            const Chord& c = geo.chords[i];
            const double tw = remap_cyclic(indexer_value(style.stroke.width_indexer,
                                                          c.a, c.b, i, N, max_len),
                                            style.cyclic);
            Color col_a, col_b;
            if (opts.plotter_mode) {
                col_a = col_b = Color{};
            } else if (have_overrides) {
                col_a = geo.chord_color_overrides[i];
                col_b = have_gradient ? geo.chord_end_color_overrides[i] : col_a;
            } else {
                const double tc = remap_cyclic(indexer_value(style.color_indexer,
                                                              c.a, c.b, i, N, max_len),
                                                style.cyclic);
                col_a = style.color_map->at(tc);
                col_b = col_a;
            }
            const double w = lerp_width(style.stroke.width_min, style.stroke.width_max, tw);

            if (col_a == col_b) {
                const std::string hex = opts.plotter_mode ? opts.plotter_color : color_to_hex(col_a);
                const double op = col_a.a * style.stroke.opacity;
                emit_line(out, to_svg(m, c.a), to_svg(m, c.b), hex, w, op, with_opacity);
            } else {
                // Gradient: subdivide into 16 sub-segments with lerped colour.
                constexpr int kSubsegments = 16;
                for (int s = 0; s < kSubsegments; ++s) {
                    const double t0 = static_cast<double>(s)     / kSubsegments;
                    const double t1 = static_cast<double>(s + 1) / kSubsegments;
                    const Vec2 p0{c.a.x + (c.b.x - c.a.x) * t0, c.a.y + (c.b.y - c.a.y) * t0};
                    const Vec2 p1{c.a.x + (c.b.x - c.a.x) * t1, c.a.y + (c.b.y - c.a.y) * t1};
                    const double tmid = (t0 + t1) * 0.5;
                    const Color seg_col = lerp(col_a, col_b, tmid);
                    const std::string hex = color_to_hex(seg_col);
                    const double op = seg_col.a * style.stroke.opacity;
                    emit_line(out, to_svg(m, p0), to_svg(m, p1), hex, w, op, with_opacity);
                }
            }
        }
    }

    // Polylines -----------------------------------------------------------
    for (const auto& p : geo.polylines) {
        if (p.size() < 2) continue;

        if (opts.plotter_mode) {
            const double w = (style.stroke.width_min + style.stroke.width_max) * 0.5;
            emit_polyline(out, p, m, opts.plotter_color, w, 1.0, false);
            continue;
        }

        const std::size_t segs = p.size() - 1;
        const double max_len = max_polyline_segment_length(p);
        for (std::size_t i = 0; i < segs; ++i) {
            const Vec2 a = p[i];
            const Vec2 b = p[i + 1];
            const double tc = remap_cyclic(indexer_value(style.color_indexer,
                                                          a, b, i, segs, max_len),
                                            style.cyclic);
            const double tw = remap_cyclic(indexer_value(style.stroke.width_indexer,
                                                          a, b, i, segs, max_len),
                                            style.cyclic);
            const Color col = style.color_map->at(tc);
            const double w = lerp_width(style.stroke.width_min, style.stroke.width_max, tw);
            const double op = col.a * style.stroke.opacity;
            emit_line(out, to_svg(m, a), to_svg(m, b), color_to_hex(col), w, op, with_opacity);
        }
    }

    // Points (scatter) — used by attractor generators in Scatter / Both
    // render modes. Each entry emits a single <circle>; radius derives from
    // stroke.width_min with a small floor so sub-pixel widths still render.
    if (!geo.points.empty()) {
        const std::size_t N = geo.points.size();
        const double radius = std::max(0.3, style.stroke.width_min * 0.5);
        for (std::size_t i = 0; i < N; ++i) {
            const Vec2 sv = to_svg(m, geo.points[i]);
            Color col;
            if (opts.plotter_mode) {
                col = Color{};
            } else {
                const double t  = (N > 1) ? static_cast<double>(i) /
                                            static_cast<double>(N - 1) : 0.0;
                const double tc = remap_cyclic(t, style.cyclic);
                col = style.color_map->at(tc);
            }
            const std::string hex = opts.plotter_mode ? opts.plotter_color : color_to_hex(col);
            const double op = col.a * style.stroke.opacity;
            out << "    <circle cx=\"" << fmt_f(sv.x)
                << "\" cy=\"" << fmt_f(sv.y)
                << "\" r=\""  << fmt_f(radius)
                << "\" fill=\"" << hex << "\"";
            if (with_opacity) {
                out << " fill-opacity=\"" << fmt_f(op) << "\"";
            }
            out << "/>\n";
        }
    }

    out << "  </g>\n";
}

}  // namespace

std::string render_svg(const std::vector<LayerRender>& layers,
                       Color background,
                       const SvgOptions& opts) {
    const Bounds b = compute_bounds(layers);
    const Mapping m = make_mapping(b, opts);
    const bool with_opacity = !opts.plotter_mode;

    std::ostringstream out;
    out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        << "<svg xmlns=\"http://www.w3.org/2000/svg\""
        << " xmlns:inkscape=\"http://www.inkscape.org/namespaces/inkscape\""
        << " viewBox=\"0 0 " << fmt_f(opts.width) << ' ' << fmt_f(opts.height) << "\""
        << " width=\"" << fmt_f(opts.width) << "\""
        << " height=\"" << fmt_f(opts.height) << "\">\n";

    if (!opts.plotter_mode) {
        out << "  <rect x=\"0\" y=\"0\" width=\"" << fmt_f(opts.width)
            << "\" height=\"" << fmt_f(opts.height)
            << "\" fill=\"" << color_to_hex(background) << "\"/>\n";
    }

    for (std::size_t i = 0; i < layers.size(); ++i) {
        render_layer(out, i, layers[i], m, opts, with_opacity);
    }

    out << "</svg>\n";
    return out.str();
}

void write_svg(const std::filesystem::path& path,
               const std::vector<LayerRender>& layers,
               Color background,
               const SvgOptions& opts) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot open for write: " + path.string());
    f << render_svg(layers, background, opts);
}

}  // namespace caustic
