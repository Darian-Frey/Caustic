#include <algorithm>
#include <cmath>
#include <memory>
#include <numbers>

#include <caustic/camera.hpp>
#include <caustic/colormap.hpp>
#include <caustic/generators/epitrochoid.hpp>
#include <caustic/generators/hypotrochoid.hpp>
#include <caustic/generators/lissajous.hpp>
#include <caustic/generators/modular_chord.hpp>
#include <caustic/geometry_buffer.hpp>
#include <caustic/sampler.hpp>
#include <caustic/style.hpp>

#include "raylib_renderer.hpp"

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

namespace {

// ---------------------------------------------------------------------------
// State

enum class Generator {
    ModularChord,
    Hypotrochoid,
    Epitrochoid,
    Lissajous,
};

const char* const kGeneratorNames[] = {
    "modular chord", "hypotrochoid", "epitrochoid", "lissajous",
};

const char* const kIndexerNames[] = {
    "chord index", "chord length", "angle", "curve t",
};

enum class ColorMapType { Solid, LinearGradient, HsvSweep, Diverging };
const char* const kColorMapNames[] = {
    "solid", "linear gradient", "hsv sweep", "diverging",
};

struct ChordParams { int N = 200; double k = 2.0; };
struct HypoParams { double R = 5.0; double r = 3.0; double d = 2.0; int samples = 4000; };
struct EpiParams  { double R = 3.0; double r = 1.0; double d = 1.5; int samples = 4000; };
struct LissParams {
    double A = 1.0; double B = 1.0;
    double a = 3.0; double b = 2.0;
    double phi = std::numbers::pi / 2.0;
    int samples = 4000;
};

struct StyleSpec {
    ColorMapType colormap_type = ColorMapType::HsvSweep;

    caustic::Color solid_color    = {1.00, 1.00, 1.00, 1.0};
    caustic::Color gradient_start = {0.10, 0.27, 0.50, 1.0};
    caustic::Color gradient_end   = {0.94, 0.75, 0.31, 1.0};
    double hue_start = 0.0;
    double hue_end   = 360.0;
    double hsv_saturation = 0.85;
    double hsv_value      = 0.95;
    caustic::Color div_negative = {0.95, 0.55, 0.20, 1.0};
    caustic::Color div_midpoint = {0.95, 0.95, 0.95, 1.0};
    caustic::Color div_positive = {0.20, 0.70, 0.85, 1.0};

    caustic::Indexer color_indexer = caustic::Indexer::ChordLength;

    double stroke_width_min = 0.8;
    double stroke_width_max = 0.8;
    caustic::Indexer stroke_width_indexer = caustic::Indexer::ChordIndex;
    double opacity = 0.6;

    caustic::Color background = {0.04, 0.04, 0.04, 1.0};

    bool cyclic = false;
};

struct AppState {
    Generator gen = Generator::ModularChord;
    ChordParams chord;
    HypoParams  hypo;
    EpiParams   epi;
    LissParams  liss;
    StyleSpec   style;
    caustic::CameraState camera;

    bool dirty = true;
    bool any_active_last_frame = false;
    bool any_active_two_frames_ago = false;
    bool last_render_was_coarse = false;
};

// ---------------------------------------------------------------------------
// Geometry + Style construction

caustic::GeometryBuffer build_geometry(const AppState& state, bool coarse) {
    caustic::GeometryBuffer geo;
    switch (state.gen) {
        case Generator::ModularChord: {
            const int N = coarse ? std::max(3, state.chord.N / 4) : state.chord.N;
            geo.chords = caustic::modular_chord(N, state.chord.k);
            break;
        }
        case Generator::Hypotrochoid: {
            caustic::HypotrochoidCurve curve(state.hypo.R, state.hypo.r, state.hypo.d);
            const int n = coarse ? std::max(100, state.hypo.samples / 2) : state.hypo.samples;
            geo.polylines.push_back(caustic::sample_curve(curve, n));
            break;
        }
        case Generator::Epitrochoid: {
            caustic::EpitrochoidCurve curve(state.epi.R, state.epi.r, state.epi.d);
            const int n = coarse ? std::max(100, state.epi.samples / 2) : state.epi.samples;
            geo.polylines.push_back(caustic::sample_curve(curve, n));
            break;
        }
        case Generator::Lissajous: {
            caustic::LissajousCurve curve(state.liss.A, state.liss.B, state.liss.a, state.liss.b, state.liss.phi);
            const int n = coarse ? std::max(100, state.liss.samples / 2) : state.liss.samples;
            geo.polylines.push_back(caustic::sample_curve(curve, n));
            break;
        }
    }
    return geo;
}

caustic::Style build_style(const StyleSpec& spec) {
    caustic::Style s;
    switch (spec.colormap_type) {
        case ColorMapType::Solid:
            s.color_map = std::make_shared<caustic::Solid>(spec.solid_color);
            break;
        case ColorMapType::LinearGradient:
            s.color_map = std::make_shared<caustic::LinearGradient>(spec.gradient_start, spec.gradient_end);
            break;
        case ColorMapType::HsvSweep:
            s.color_map = std::make_shared<caustic::HsvSweep>(spec.hue_start, spec.hue_end, spec.hsv_saturation, spec.hsv_value);
            break;
        case ColorMapType::Diverging:
            s.color_map = std::make_shared<caustic::Diverging>(spec.div_negative, spec.div_midpoint, spec.div_positive);
            break;
    }
    s.color_indexer = spec.color_indexer;
    s.stroke = {spec.stroke_width_min, spec.stroke_width_max, spec.stroke_width_indexer, spec.opacity};
    s.background = spec.background;
    s.cyclic = spec.cyclic;
    return s;
}

// ---------------------------------------------------------------------------
// Scroll-wheel-on-hover slider helpers
//
// CLAUDE.md convention: scroll = ±1 step, Shift+scroll = ×10, Ctrl+scroll = ×0.1.
// Disambiguation rule: scroll-on-slider belongs to the slider, scroll-on-canvas
// belongs to the camera. The slider helpers check IsItemHovered; the camera
// scroll handler in main() checks !io.WantCaptureMouse, so they don't fight.

bool wheel_adjust_double(double* value, double step, double lo, double hi) {
    if (!ImGui::IsItemHovered()) return false;
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel == 0.0f) return false;
    double mult = 1.0;
    if (ImGui::GetIO().KeyShift) mult = 10.0;
    else if (ImGui::GetIO().KeyCtrl) mult = 0.1;
    const double next = std::clamp(*value + static_cast<double>(wheel) * step * mult, lo, hi);
    if (next == *value) return false;
    *value = next;
    return true;
}

bool wheel_adjust_int(int* value, int step, int lo, int hi) {
    if (!ImGui::IsItemHovered()) return false;
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel == 0.0f) return false;
    int actual = step;
    if (ImGui::GetIO().KeyShift) actual = step * 10;
    else if (ImGui::GetIO().KeyCtrl) actual = std::max(1, step / 10);
    const int next = std::clamp(*value + static_cast<int>(wheel) * actual, lo, hi);
    if (next == *value) return false;
    *value = next;
    return true;
}

bool slider_int_w(const char* label, int* v, int lo, int hi) {
    bool changed = ImGui::SliderInt(label, v, lo, hi);
    if (wheel_adjust_int(v, 1, lo, hi)) changed = true;
    return changed;
}

bool slider_double_w(const char* label, double* v, double lo, double hi, double step, const char* fmt = "%.3f") {
    bool changed = ImGui::SliderScalar(label, ImGuiDataType_Double, v, &lo, &hi, fmt);
    if (wheel_adjust_double(v, step, lo, hi)) changed = true;
    return changed;
}

bool color_edit_double(const char* label, caustic::Color* c) {
    float rgb[3] = {static_cast<float>(c->r), static_cast<float>(c->g), static_cast<float>(c->b)};
    if (ImGui::ColorEdit3(label, rgb)) {
        c->r = rgb[0]; c->g = rgb[1]; c->b = rgb[2];
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// UI panels

void render_param_panel(AppState& state) {
    ImGui::Begin("Parameters");

    int gen_idx = static_cast<int>(state.gen);
    if (ImGui::Combo("Generator", &gen_idx, kGeneratorNames, IM_ARRAYSIZE(kGeneratorNames))) {
        state.gen = static_cast<Generator>(gen_idx);
        state.dirty = true;
    }

    ImGui::Separator();

    switch (state.gen) {
        case Generator::ModularChord:
            if (slider_int_w("N", &state.chord.N, 3, 1000)) state.dirty = true;
            if (slider_double_w("k", &state.chord.k, 0.0, 100.0, 0.01)) state.dirty = true;
            break;
        case Generator::Hypotrochoid: {
            // R and r are integer-only so R/r is always a small-denominator
            // rational and the curve closes after gcd(R,r) revolutions.
            // Non-integer ratios precess and fill an annulus — the killer-demo
            // behavior described in ARCHITECTURE.md but not what the SPEC
            // defaults look like; left as a future toggle.
            //
            // SPEC.md: r ∈ (0, R). When r = R the curve collapses to the point
            // (d, 0). Enforce r ≤ R - 1 here (and R ≥ 2 so r ≥ 1 stays valid).
            int Ri = static_cast<int>(std::lround(state.hypo.R));
            int ri = static_cast<int>(std::lround(state.hypo.r));
            if (slider_int_w("R", &Ri, 2, 10)) { state.hypo.R = static_cast<double>(Ri); state.dirty = true; }
            const int r_max = Ri - 1;
            if (ri > r_max) { ri = r_max; state.hypo.r = static_cast<double>(ri); state.dirty = true; }
            if (slider_int_w("r", &ri, 1, r_max)) { state.hypo.r = static_cast<double>(ri); state.dirty = true; }
            if (slider_double_w("d", &state.hypo.d, 0.0, 2.0 * state.hypo.R, 0.01)) state.dirty = true;
            if (slider_int_w("samples", &state.hypo.samples, 100, 100000)) state.dirty = true;
            break;
        }
        case Generator::Epitrochoid: {
            int Ri = static_cast<int>(std::lround(state.epi.R));
            int ri = static_cast<int>(std::lround(state.epi.r));
            if (slider_int_w("R", &Ri, 1, 10)) { state.epi.R = static_cast<double>(Ri); state.dirty = true; }
            if (slider_int_w("r", &ri, 1, 10)) { state.epi.r = static_cast<double>(ri); state.dirty = true; }
            if (slider_double_w("d", &state.epi.d, 0.0, 3.0 * state.epi.R, 0.01)) state.dirty = true;
            if (slider_int_w("samples", &state.epi.samples, 100, 100000)) state.dirty = true;
            break;
        }
        case Generator::Lissajous: {
            int ai = static_cast<int>(std::lround(state.liss.a));
            int bi = static_cast<int>(std::lround(state.liss.b));
            if (slider_double_w("A", &state.liss.A, 0.1, 5.0, 0.01)) state.dirty = true;
            if (slider_double_w("B", &state.liss.B, 0.1, 5.0, 0.01)) state.dirty = true;
            // a, b integer-only so a/b is rational and the figure closes.
            if (slider_int_w("a", &ai, 1, 50)) { state.liss.a = static_cast<double>(ai); state.dirty = true; }
            if (slider_int_w("b", &bi, 1, 50)) { state.liss.b = static_cast<double>(bi); state.dirty = true; }
            if (slider_double_w("phi", &state.liss.phi, 0.0, 2.0 * std::numbers::pi, 0.01)) state.dirty = true;
            if (slider_int_w("samples", &state.liss.samples, 100, 100000)) state.dirty = true;
            break;
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("keys 1–4: switch generator   F or 0: reset camera");
    ImGui::TextDisabled("middle drag: pan   scroll on canvas: zoom");

    ImGui::End();
}

void render_style_panel(AppState& state) {
    StyleSpec& s = state.style;
    ImGui::Begin("Style");

    int cm = static_cast<int>(s.colormap_type);
    if (ImGui::Combo("Color map", &cm, kColorMapNames, IM_ARRAYSIZE(kColorMapNames))) {
        s.colormap_type = static_cast<ColorMapType>(cm);
        state.dirty = true;
    }

    switch (s.colormap_type) {
        case ColorMapType::Solid:
            if (color_edit_double("color", &s.solid_color)) state.dirty = true;
            break;
        case ColorMapType::LinearGradient:
            if (color_edit_double("start", &s.gradient_start)) state.dirty = true;
            if (color_edit_double("end",   &s.gradient_end))   state.dirty = true;
            break;
        case ColorMapType::HsvSweep:
            if (slider_double_w("hue start",  &s.hue_start,      0.0, 360.0, 1.0, "%.0f")) state.dirty = true;
            if (slider_double_w("hue end",    &s.hue_end,        0.0, 360.0, 1.0, "%.0f")) state.dirty = true;
            if (slider_double_w("saturation", &s.hsv_saturation, 0.0,   1.0, 0.01))         state.dirty = true;
            if (slider_double_w("value",      &s.hsv_value,      0.0,   1.0, 0.01))         state.dirty = true;
            break;
        case ColorMapType::Diverging:
            if (color_edit_double("negative", &s.div_negative)) state.dirty = true;
            if (color_edit_double("midpoint", &s.div_midpoint)) state.dirty = true;
            if (color_edit_double("positive", &s.div_positive)) state.dirty = true;
            break;
    }

    int ci = static_cast<int>(s.color_indexer);
    if (ImGui::Combo("Color indexer", &ci, kIndexerNames, IM_ARRAYSIZE(kIndexerNames))) {
        s.color_indexer = static_cast<caustic::Indexer>(ci);
        state.dirty = true;
    }

    ImGui::Separator();
    ImGui::Text("Stroke");

    if (slider_double_w("width min", &s.stroke_width_min, 0.1, 10.0, 0.05)) state.dirty = true;
    if (slider_double_w("width max", &s.stroke_width_max, 0.1, 10.0, 0.05)) state.dirty = true;

    int wi = static_cast<int>(s.stroke_width_indexer);
    if (ImGui::Combo("width indexer", &wi, kIndexerNames, IM_ARRAYSIZE(kIndexerNames))) {
        s.stroke_width_indexer = static_cast<caustic::Indexer>(wi);
        state.dirty = true;
    }

    if (slider_double_w("opacity", &s.opacity, 0.0, 1.0, 0.01)) state.dirty = true;

    ImGui::Separator();

    if (color_edit_double("background", &s.background)) state.dirty = true;
    if (ImGui::Checkbox("cyclic (closed-curve continuity)", &s.cyclic)) state.dirty = true;

    ImGui::End();
}

}  // namespace

int main() {
    constexpr int kWidth = 1280;
    constexpr int kHeight = 800;

    InitWindow(kWidth, kHeight, "Caustic");
    SetTargetFPS(60);
    rlImGuiSetup(true);

    caustic::RaylibRenderer renderer(kWidth, kHeight);
    AppState state;

    while (!WindowShouldClose()) {
        const ImGuiIO& io = ImGui::GetIO();

        // -- Camera input (only when ImGui isn't capturing the mouse) --------
        if (!io.WantCaptureMouse) {
            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
                const Vector2 d = GetMouseDelta();
                if (d.x != 0.0f || d.y != 0.0f) {
                    state.camera.pan_x_px += d.x;
                    state.camera.pan_y_px += d.y;
                    state.dirty = true;
                }
            }
            const float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                const Vector2 m = GetMousePosition();
                const double cx = kWidth / 2.0;
                const double cy = kHeight / 2.0;
                const double factor = (wheel > 0.0f) ? 1.1 : (1.0 / 1.1);
                const double new_zoom = std::clamp(state.camera.zoom * factor, 0.1, 100.0);
                const double effective = new_zoom / state.camera.zoom;
                state.camera.pan_x_px = (m.x - cx) * (1.0 - effective) + state.camera.pan_x_px * effective;
                state.camera.pan_y_px = (m.y - cy) * (1.0 - effective) + state.camera.pan_y_px * effective;
                state.camera.zoom = new_zoom;
                state.dirty = true;
            }
        }

        // -- Keyboard shortcuts (only when ImGui isn't capturing keys) -------
        if (!io.WantCaptureKeyboard) {
            const Generator prev = state.gen;
            if (IsKeyPressed(KEY_ONE))   state.gen = Generator::ModularChord;
            if (IsKeyPressed(KEY_TWO))   state.gen = Generator::Hypotrochoid;
            if (IsKeyPressed(KEY_THREE)) state.gen = Generator::Epitrochoid;
            if (IsKeyPressed(KEY_FOUR))  state.gen = Generator::Lissajous;
            if (state.gen != prev) state.dirty = true;

            if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_ZERO)) {
                state.camera = caustic::CameraState{};
                state.dirty = true;
            }
        }

        // -- Decide whether to redraw the canvas this frame ------------------
        // `dragging` and `just_released` use last-frame ImGui state — correct
        // for the redraw that feeds *this* frame's blit. There's a 1-frame lag
        // on quality-upgrade after slider release; imperceptible at 60fps.
        const bool dragging = state.any_active_last_frame;
        const bool just_released = state.any_active_two_frames_ago && !dragging;
        if (state.dirty || (just_released && state.last_render_was_coarse)) {
            const bool coarse = dragging;
            renderer.redraw(build_geometry(state, coarse), build_style(state.style), state.camera);
            state.dirty = false;
            state.last_render_was_coarse = coarse;
        }

        // -- Render the screen ----------------------------------------------
        BeginDrawing();
        ClearBackground(BLACK);
        renderer.blit_to_screen();

        rlImGuiBegin();
        render_param_panel(state);
        render_style_panel(state);
        const bool active_now = ImGui::IsAnyItemActive();
        rlImGuiEnd();

        EndDrawing();

        state.any_active_two_frames_ago = state.any_active_last_frame;
        state.any_active_last_frame = active_now;
    }

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
