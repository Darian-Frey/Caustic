#include "raylib_renderer.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include <caustic/indexer.hpp>
#include <caustic/vec2.hpp>

namespace caustic {

namespace {

double max_extent(const std::vector<LayerRender>& layers) {
    double m = 0.0;
    auto track = [&](Vec2 v) {
        m = std::max(m, std::max(std::abs(v.x), std::abs(v.y)));
    };
    for (const auto& L : layers) {
        for (const auto& c : L.geometry.chords) { track(c.a); track(c.b); }
        for (const auto& p : L.geometry.polylines) {
            for (const auto& v : p) track(v);
        }
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

void RaylibRenderer::resize(int width, int height) {
    if (width <= 0 || height <= 0) return;
    if (width == width_ && height == height_) return;
    UnloadRenderTexture(canvas_);
    width_ = width;
    height_ = height;
    canvas_ = LoadRenderTexture(width_, height_);
}

void RaylibRenderer::redraw(const std::vector<LayerRender>& layers,
                            Color background,
                            const CameraState& camera) {
    double extent = max_extent(layers);
    if (extent < 1e-9) extent = 1.0;

    const double fit_scale = std::min(width_, height_) * 0.45 / extent;
    last_fit_scale_ = fit_scale;
    const double scale = fit_scale * camera.zoom;
    const double cx = width_ / 2.0;
    const double cy = height_ / 2.0;

    auto to_screen = [&](Vec2 v) -> Vector2 {
        return {
            static_cast<float>(cx + camera.pan_x_px + v.x * scale),
            static_cast<float>(cy + camera.pan_y_px - v.y * scale),
        };
    };

    BeginTextureMode(canvas_);
    ClearBackground(to_raylib(background, 1.0));

    for (const auto& L : layers) {
        if (!L.style.color_map) continue;

        // Chords
        if (!L.geometry.chords.empty()) {
            const std::size_t N = L.geometry.chords.size();
            const double max_len = max_chord_length(L.geometry.chords);
            const bool have_overrides =
                L.geometry.chord_color_overrides.size() == N;
            const bool have_gradient =
                L.geometry.chord_end_color_overrides.size() == N;
            const bool have_widths =
                L.geometry.chord_width_overrides.size() == N;
            const bool have_opacities =
                L.geometry.chord_opacity_overrides.size() == N;
            for (std::size_t i = 0; i < N; ++i) {
                const Chord& c = L.geometry.chords[i];
                const double tw = remap_cyclic(indexer_value(L.style.stroke.width_indexer, c.a, c.b, i, N, max_len), L.style.cyclic);
                Color col_a, col_b;
                if (have_overrides) {
                    col_a = L.geometry.chord_color_overrides[i];
                    col_b = have_gradient ? L.geometry.chord_end_color_overrides[i] : col_a;
                } else {
                    const double tc = remap_cyclic(indexer_value(L.style.color_indexer, c.a, c.b, i, N, max_len), L.style.cyclic);
                    col_a = L.style.color_map->at(tc);
                    col_b = col_a;
                }
                const double width_v = have_widths
                    ? L.geometry.chord_width_overrides[i]
                    : lerp_width(L.style.stroke.width_min, L.style.stroke.width_max, tw);
                const float w = static_cast<float>(width_v);
                const double op = have_opacities
                    ? L.geometry.chord_opacity_overrides[i]
                    : L.style.stroke.opacity;
                if (col_a == col_b) {
                    DrawLineEx(to_screen(c.a), to_screen(c.b), w, to_raylib(col_a, op));
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
                        DrawLineEx(to_screen(p0), to_screen(p1), w, to_raylib(seg_col, op));
                    }
                }
            }
        }

        // Polylines
        for (const auto& p : L.geometry.polylines) {
            if (p.size() < 2) continue;
            const std::size_t segs = p.size() - 1;
            const double max_len = max_polyline_segment_length(p);
            for (std::size_t i = 0; i < segs; ++i) {
                const Vec2 a = p[i];
                const Vec2 b = p[i + 1];
                const double tc = remap_cyclic(indexer_value(L.style.color_indexer, a, b, i, segs, max_len), L.style.cyclic);
                const double tw = remap_cyclic(indexer_value(L.style.stroke.width_indexer, a, b, i, segs, max_len), L.style.cyclic);
                const Color col = L.style.color_map->at(tc);
                const float w = static_cast<float>(lerp_width(L.style.stroke.width_min, L.style.stroke.width_max, tw));
                DrawLineEx(to_screen(a), to_screen(b), w, to_raylib(col, L.style.stroke.opacity));
            }
        }

        // Points (scatter) — used by strange-attractor generators in
        // Scatter / Both render modes. Each entry draws as a small filled
        // circle, coloured by the style's color_map sampled at i / (N-1).
        // Dot radius derives from stroke.width_min with a small floor so
        // sub-pixel widths still produce a visible dot.
        if (!L.geometry.points.empty()) {
            const std::size_t N = L.geometry.points.size();
            const float radius = std::max(0.5f,
                static_cast<float>(L.style.stroke.width_min * 0.5));
            for (std::size_t i = 0; i < N; ++i) {
                const double t = (N > 1) ? static_cast<double>(i) /
                                            static_cast<double>(N - 1) : 0.0;
                const double tc = remap_cyclic(t, L.style.cyclic);
                const Color col = L.style.color_map->at(tc);
                DrawCircleV(to_screen(L.geometry.points[i]), radius,
                            to_raylib(col, L.style.stroke.opacity));
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

bool RaylibRenderer::write_png(const char* path) const {
    Image img = LoadImageFromTexture(canvas_.texture);
    // RenderTexture pixels come out bottom-up; flip so the saved image is
    // right-way-up.
    ImageFlipVertical(&img);
    const bool ok = ExportImage(img, path);
    UnloadImage(img);
    return ok;
}

bool RaylibRenderer::read_rgba_frame(unsigned char** out_pixels,
                                     int* out_w, int* out_h) const {
    if (!out_pixels || !out_w || !out_h) return false;
    Image img = LoadImageFromTexture(canvas_.texture);
    ImageFlipVertical(&img);
    if (img.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ImageFormat(&img, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    const std::size_t bytes = static_cast<std::size_t>(img.width) *
                              static_cast<std::size_t>(img.height) * 4;
    unsigned char* buf = static_cast<unsigned char*>(std::malloc(bytes));
    if (!buf) { UnloadImage(img); return false; }
    std::memcpy(buf, img.data, bytes);
    *out_pixels = buf;
    *out_w = img.width;
    *out_h = img.height;
    UnloadImage(img);
    return true;
}

}  // namespace caustic
