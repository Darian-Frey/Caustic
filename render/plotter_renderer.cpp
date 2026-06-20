#include "plotter_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <caustic/vec2.hpp>

namespace caustic {

namespace {

// One pen-down sequence: 2+ points connected by line segments. Chords become
// 2-point paths; polylines become N-point paths. Scatter points are skipped
// (no plotter-meaningful representation).
struct PlotterPath {
    std::vector<Vec2> points;
};

struct Bounds {
    double xmin = 0, xmax = 0, ymin = 0, ymax = 0;
    bool   empty = true;
};

void include_point(Bounds& b, Vec2 v) {
    if (b.empty) {
        b.xmin = b.xmax = v.x;
        b.ymin = b.ymax = v.y;
        b.empty = false;
        return;
    }
    if (v.x < b.xmin) b.xmin = v.x;
    if (v.x > b.xmax) b.xmax = v.x;
    if (v.y < b.ymin) b.ymin = v.y;
    if (v.y > b.ymax) b.ymax = v.y;
}

std::vector<PlotterPath> extract_paths(const std::vector<LayerRender>& layers,
                                      Bounds& bounds) {
    std::vector<PlotterPath> paths;
    for (const auto& L : layers) {
        for (const auto& c : L.geometry.chords) {
            paths.push_back(PlotterPath{{c.a, c.b}});
            include_point(bounds, c.a);
            include_point(bounds, c.b);
        }
        for (const auto& p : L.geometry.polylines) {
            if (p.size() < 2) continue;
            paths.push_back(PlotterPath{p});
            for (const auto& v : p) include_point(bounds, v);
        }
        // Skipped: L.geometry.points. Plotters can't draw a true point —
        // would need to emit a tiny stroke pattern, which is uglier than
        // omitting them.
    }
    if (bounds.empty) {
        bounds.xmin = bounds.ymin = -1.0;
        bounds.xmax = bounds.ymax =  1.0;
        bounds.empty = false;
    }
    return paths;
}

// Lexicographic sort by first point — same nearest-neighbour approximation
// svg_renderer's plotter mode uses. A true 2-opt / TSP would be better but
// is out of scope here; this keeps the pen from sprinting all over the bed
// for free.
void sort_paths_for_pen_travel(std::vector<PlotterPath>& paths) {
    std::sort(paths.begin(), paths.end(),
              [](const PlotterPath& a, const PlotterPath& b) {
                  if (a.points.empty()) return false;
                  if (b.points.empty()) return true;
                  const Vec2& pa = a.points.front();
                  const Vec2& pb = b.points.front();
                  if (pa.x != pb.x) return pa.x < pb.x;
                  return pa.y < pb.y;
              });
}

struct PageFit {
    double scale;          // world units → mm
    double world_cx;
    double world_cy;
    double page_cx_mm;
    double page_cy_mm;
};

PageFit make_page_fit(const Bounds& b, const PlotterOptions& opts) {
    PageFit f;
    const double bbox_w    = std::max(1e-9, b.xmax - b.xmin);
    const double bbox_h    = std::max(1e-9, b.ymax - b.ymin);
    const double margin_mm = opts.margin * std::min(opts.width_mm, opts.height_mm);
    const double avail_w   = std::max(1.0, opts.width_mm  - 2.0 * margin_mm);
    const double avail_h   = std::max(1.0, opts.height_mm - 2.0 * margin_mm);
    f.scale      = std::min(avail_w / bbox_w, avail_h / bbox_h);
    f.world_cx   = (b.xmin + b.xmax) / 2.0;
    f.world_cy   = (b.ymin + b.ymax) / 2.0;
    f.page_cx_mm = opts.width_mm  / 2.0;
    f.page_cy_mm = opts.height_mm / 2.0;
    return f;
}

inline Vec2 to_page_mm(const PageFit& f, Vec2 v) {
    return {
        f.page_cx_mm + (v.x - f.world_cx) * f.scale,
        f.page_cy_mm + (v.y - f.world_cy) * f.scale,
    };
}

// Fixed precision — same determinism guarantee as svg_renderer's fmt_f.
std::string fmt_mm(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.3f", v);
    return buf;
}

std::string fmt_pu(double v) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%d",
                  static_cast<int>(std::lround(v)));
    return buf;
}

}  // namespace

std::string render_gcode(const std::vector<LayerRender>& layers,
                         const PlotterOptions& opts) {
    Bounds bounds;
    std::vector<PlotterPath> paths = extract_paths(layers, bounds);
    sort_paths_for_pen_travel(paths);
    const PageFit fit = make_page_fit(bounds, opts);

    std::ostringstream out;
    out << "; Caustic G-code (Grbl flavour, pen plotter)\n"
        << "; page: " << fmt_mm(opts.width_mm)  << " x "
                      << fmt_mm(opts.height_mm) << " mm\n"
        << "; paths: " << paths.size() << "\n"
        << "G21              ; mm\n"
        << "G90              ; absolute coordinates\n"
        << "G17              ; XY plane\n"
        << "G94              ; units/min feed mode\n"
        << "G0 Z" << fmt_mm(opts.pen_up_z)
        << " F" << fmt_mm(opts.plunge_feedrate) << "    ; pen up\n";

    for (const auto& path : paths) {
        if (path.points.size() < 2) continue;
        const Vec2 first = to_page_mm(fit, path.points.front());
        out << "G0 X" << fmt_mm(first.x)
            << " Y"   << fmt_mm(first.y)
            << " F"   << fmt_mm(opts.travel_feedrate) << "\n";
        out << "G1 Z" << fmt_mm(opts.pen_down_z)
            << " F"   << fmt_mm(opts.plunge_feedrate) << "    ; pen down\n";
        for (std::size_t i = 1; i < path.points.size(); ++i) {
            const Vec2 p = to_page_mm(fit, path.points[i]);
            out << "G1 X" << fmt_mm(p.x)
                << " Y"   << fmt_mm(p.y)
                << " F"   << fmt_mm(opts.draw_feedrate) << "\n";
        }
        out << "G0 Z" << fmt_mm(opts.pen_up_z)
            << " F"   << fmt_mm(opts.plunge_feedrate) << "    ; pen up\n";
    }

    out << "G0 X0 Y0 F" << fmt_mm(opts.travel_feedrate) << "    ; return home\n"
        << "M2               ; program end\n";
    return out.str();
}

std::string render_hpgl(const std::vector<LayerRender>& layers,
                        const PlotterOptions& opts) {
    Bounds bounds;
    std::vector<PlotterPath> paths = extract_paths(layers, bounds);
    sort_paths_for_pen_travel(paths);
    const PageFit fit = make_page_fit(bounds, opts);

    // 40 plotter units per mm is the HP-GL standard for HP plotters.
    constexpr double kPlotterUnitsPerMm = 40.0;

    auto to_pu = [&](Vec2 v) {
        const Vec2 mm = to_page_mm(fit, v);
        return Vec2{mm.x * kPlotterUnitsPerMm, mm.y * kPlotterUnitsPerMm};
    };

    std::ostringstream out;
    out << "IN;\n"                    // Initialize
        << "SP" << opts.pen_number    // Select pen
        << ";\n";

    for (const auto& path : paths) {
        if (path.points.size() < 2) continue;
        const Vec2 first = to_pu(path.points.front());
        // Pen up + move to first point.
        out << "PU" << fmt_pu(first.x) << "," << fmt_pu(first.y) << ";\n";
        // Pen down + draw to each subsequent point on one PD.
        out << "PD";
        for (std::size_t i = 1; i < path.points.size(); ++i) {
            const Vec2 p = to_pu(path.points[i]);
            if (i > 1) out << ",";
            out << fmt_pu(p.x) << "," << fmt_pu(p.y);
        }
        out << ";\n";
        out << "PU;\n";  // Lift the pen before moving to the next path.
    }

    out << "SP0;\n"   // Deselect pen (carousel returns it)
        << "IN;\n";   // Reset
    return out.str();
}

void write_gcode(const std::filesystem::path& path,
                 const std::vector<LayerRender>& layers,
                 const PlotterOptions& opts) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot open for write: " + path.string());
    f << render_gcode(layers, opts);
}

void write_hpgl(const std::filesystem::path& path,
                const std::vector<LayerRender>& layers,
                const PlotterOptions& opts) {
    if (path.has_parent_path()) {
        std::filesystem::create_directories(path.parent_path());
    }
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cannot open for write: " + path.string());
    f << render_hpgl(layers, opts);
}

}  // namespace caustic
