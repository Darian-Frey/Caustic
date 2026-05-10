#include "raylib_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <caustic/color.hpp>
#include <caustic/indexer.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

namespace {

double max_extent(const GeometryBuffer& geo) {
    double m = 0.0;
    auto track = [&](Vec2 v) {
        m = std::max(m, std::max(std::abs(v.x), std::abs(v.y)));
    };
    for (const auto& c : geo.chords) { track(c.a); track(c.b); }
    for (const auto& p : geo.polylines) {
        for (const auto& v : p) track(v);
    }
    return m;
}

double segment_length(Vec2 a, Vec2 b) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    return std::sqrt(dx * dx + dy * dy);
}

double max_chord_length(const ChordSet& chords) {
    double m = 0.0;
    for (const auto& c : chords) m = std::max(m, segment_length(c.a, c.b));
    return m;
}

double max_polyline_segment_length(const std::vector<Vec2>& poly) {
    double m = 0.0;
    for (std::size_t i = 1; i < poly.size(); ++i) {
        m = std::max(m, segment_length(poly[i - 1], poly[i]));
    }
    return m;
}

unsigned char to_byte(double v) {
    const double clamped = std::clamp(v * 255.0, 0.0, 255.0);
    return static_cast<unsigned char>(clamped);
}

::Color to_raylib(Color c, double opacity_mul) {
    return {to_byte(c.r), to_byte(c.g), to_byte(c.b), to_byte(c.a * opacity_mul)};
}

double lerp_width(double w_min, double w_max, double t) {
    if (t < 0.0) t = 0.0;
    else if (t > 1.0) t = 1.0;
    return w_min + (w_max - w_min) * t;
}

double remap_cyclic(double t, bool cyclic) {
    return cyclic ? (1.0 - std::abs(2.0 * t - 1.0)) : t;
}

}  // namespace

RaylibRenderer::RaylibRenderer(int width, int height)
    : width_(width),
      height_(height),
      canvas_(LoadRenderTexture(width, height)) {}

RaylibRenderer::~RaylibRenderer() {
    UnloadRenderTexture(canvas_);
}

void RaylibRenderer::redraw(const GeometryBuffer& geo, const Style& style) {
    double extent = max_extent(geo);
    if (extent < 1e-9) extent = 1.0;

    const double scale = std::min(width_, height_) * 0.45 / extent;
    const double cx = width_ / 2.0;
    const double cy = height_ / 2.0;

    auto to_screen = [&](Vec2 v) -> Vector2 {
        return {
            static_cast<float>(cx + v.x * scale),
            static_cast<float>(cy - v.y * scale),
        };
    };

    BeginTextureMode(canvas_);
    ClearBackground(to_raylib(style.background, 1.0));

    // Chords
    if (!geo.chords.empty() && style.color_map) {
        const std::size_t N = geo.chords.size();
        const double max_len = max_chord_length(geo.chords);
        for (std::size_t i = 0; i < N; ++i) {
            const Chord& c = geo.chords[i];
            const double tc = remap_cyclic(indexer_value(style.color_indexer, c.a, c.b, i, N, max_len), style.cyclic);
            const double tw = remap_cyclic(indexer_value(style.stroke.width_indexer, c.a, c.b, i, N, max_len), style.cyclic);
            const Color col = style.color_map->at(tc);
            const float w = static_cast<float>(lerp_width(style.stroke.width_min, style.stroke.width_max, tw));
            DrawLineEx(to_screen(c.a), to_screen(c.b), w, to_raylib(col, style.stroke.opacity));
        }
    }

    // Polylines
    if (style.color_map) {
        for (const auto& p : geo.polylines) {
            if (p.size() < 2) continue;
            const std::size_t segs = p.size() - 1;
            const double max_len = max_polyline_segment_length(p);
            for (std::size_t i = 0; i < segs; ++i) {
                const Vec2 a = p[i];
                const Vec2 b = p[i + 1];
                const double tc = remap_cyclic(indexer_value(style.color_indexer, a, b, i, segs, max_len), style.cyclic);
                const double tw = remap_cyclic(indexer_value(style.stroke.width_indexer, a, b, i, segs, max_len), style.cyclic);
                const Color col = style.color_map->at(tc);
                const float w = static_cast<float>(lerp_width(style.stroke.width_min, style.stroke.width_max, tw));
                DrawLineEx(to_screen(a), to_screen(b), w, to_raylib(col, style.stroke.opacity));
            }
        }
    }

    EndTextureMode();
}

void RaylibRenderer::blit_to_screen() const {
    const Rectangle src = {
        0.0f, 0.0f,
        static_cast<float>(canvas_.texture.width),
        -static_cast<float>(canvas_.texture.height),
    };
    const Rectangle dst = {
        0.0f, 0.0f,
        static_cast<float>(width_),
        static_cast<float>(height_),
    };
    // ::Color qualifies raylib's Color since caustic::Color shadows it inside this namespace.
    DrawTexturePro(canvas_.texture, src, dst, {0.0f, 0.0f}, 0.0f, ::Color{255, 255, 255, 255});
}

}  // namespace caustic
