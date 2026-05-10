#include "raylib_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>

#include <caustic/vec2.hpp>

namespace caustic {

namespace {

// Fit-to-content scale: largest |x| or |y| anywhere in the buffer.
// Phase 4 will replace this with a proper camera (pan, zoom, fit, reset).
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

}  // namespace

RaylibRenderer::RaylibRenderer(int width, int height)
    : width_(width),
      height_(height),
      canvas_(LoadRenderTexture(width, height)) {}

RaylibRenderer::~RaylibRenderer() {
    UnloadRenderTexture(canvas_);
}

void RaylibRenderer::redraw(const GeometryBuffer& geo) {
    double extent = max_extent(geo);
    if (extent < 1e-9) extent = 1.0;

    const double scale = std::min(width_, height_) * 0.45 / extent;
    const double cx = width_ / 2.0;
    const double cy = height_ / 2.0;

    auto to_screen = [&](Vec2 v) -> Vector2 {
        return {
            static_cast<float>(cx + v.x * scale),
            static_cast<float>(cy - v.y * scale),  // flip y for screen
        };
    };

    BeginTextureMode(canvas_);
    ClearBackground(BLACK);

    for (const auto& c : geo.chords) {
        DrawLineV(to_screen(c.a), to_screen(c.b), WHITE);
    }
    for (const auto& p : geo.polylines) {
        for (std::size_t i = 1; i < p.size(); ++i) {
            DrawLineV(to_screen(p[i - 1]), to_screen(p[i]), WHITE);
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
    DrawTexturePro(canvas_.texture, src, dst, {0.0f, 0.0f}, 0.0f, WHITE);
}

}  // namespace caustic
