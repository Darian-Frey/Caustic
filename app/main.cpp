#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <memory>
#include <numbers>
#include <string>
#include <vector>

#include <caustic/colormap.hpp>
#include <caustic/generators/epitrochoid.hpp>
#include <caustic/generators/hypotrochoid.hpp>
#include <caustic/generators/lissajous.hpp>
#include <caustic/generators/modular_chord.hpp>
#include <caustic/geometry_buffer.hpp>
#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>
#include <caustic/sampler.hpp>
#include <caustic/style.hpp>

#include "raylib_renderer.hpp"
#include "svg_renderer.hpp"

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

namespace fs = std::filesystem;

namespace {

const char* const kGeneratorNames[] = {
    "modular chord", "hypotrochoid", "epitrochoid", "lissajous",
};

const char* const kIndexerNames[] = {
    "chord index", "chord length", "angle", "curve t",
};

const char* const kColorMapNames[] = {
    "solid", "linear gradient", "hsv sweep", "diverging",
};

struct AppState {
    caustic::Preset preset;

    char save_name_buf[64] = "untitled";
    char export_name_buf[64] = "export";
    bool export_plotter_mode = false;
    int export_size = 1024;
    std::string status_message;     // shown briefly after save/load/export
    std::vector<fs::path> bundled_presets;
    std::vector<fs::path> user_presets;

    bool dirty = true;
    bool any_active_last_frame = false;
    bool any_active_two_frames_ago = false;
    bool last_render_was_coarse = false;
};

fs::path user_export_dir() {
    return caustic::user_preset_dir().parent_path() / "exports";
}

// ---------------------------------------------------------------------------
// Geometry + Style construction

caustic::GeometryBuffer build_geometry(const caustic::GeneratorSpec& g, bool coarse) {
    caustic::GeometryBuffer geo;
    switch (g.type) {
        case caustic::GeneratorType::ModularChord: {
            const int N = coarse ? std::max(3, g.chord.N / 4) : g.chord.N;
            geo.chords = caustic::modular_chord(N, g.chord.k);
            break;
        }
        case caustic::GeneratorType::Hypotrochoid: {
            caustic::HypotrochoidCurve curve(g.hypo.R, g.hypo.r, g.hypo.d);
            const int n = coarse ? std::max(100, g.hypo.samples / 2) : g.hypo.samples;
            geo.polylines.push_back(caustic::sample_curve(curve, n));
            break;
        }
        case caustic::GeneratorType::Epitrochoid: {
            caustic::EpitrochoidCurve curve(g.epi.R, g.epi.r, g.epi.d);
            const int n = coarse ? std::max(100, g.epi.samples / 2) : g.epi.samples;
            geo.polylines.push_back(caustic::sample_curve(curve, n));
            break;
        }
        case caustic::GeneratorType::Lissajous: {
            caustic::LissajousCurve curve(g.liss.A, g.liss.B, g.liss.a, g.liss.b, g.liss.phi);
            const int n = coarse ? std::max(100, g.liss.samples / 2) : g.liss.samples;
            geo.polylines.push_back(caustic::sample_curve(curve, n));
            break;
        }
    }
    return geo;
}

caustic::Style build_style(const caustic::StyleSpec& spec) {
    caustic::Style s;
    switch (spec.colormap_type) {
        case caustic::ColorMapType::Solid:
            s.color_map = std::make_shared<caustic::Solid>(spec.solid_color);
            break;
        case caustic::ColorMapType::LinearGradient:
            s.color_map = std::make_shared<caustic::LinearGradient>(spec.gradient_start, spec.gradient_end);
            break;
        case caustic::ColorMapType::HsvSweep:
            s.color_map = std::make_shared<caustic::HsvSweep>(spec.hue_start, spec.hue_end, spec.hsv_saturation, spec.hsv_value);
            break;
        case caustic::ColorMapType::Diverging:
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

bool wheel_adjust_double(double* v, double step, double lo, double hi) {
    if (!ImGui::IsItemHovered()) return false;
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel == 0.0f) return false;
    double mult = 1.0;
    if (ImGui::GetIO().KeyShift) mult = 10.0;
    else if (ImGui::GetIO().KeyCtrl) mult = 0.1;
    const double next = std::clamp(*v + static_cast<double>(wheel) * step * mult, lo, hi);
    if (next == *v) return false;
    *v = next;
    return true;
}

bool wheel_adjust_int(int* v, int step, int lo, int hi) {
    if (!ImGui::IsItemHovered()) return false;
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel == 0.0f) return false;
    int actual = step;
    if (ImGui::GetIO().KeyShift) actual = step * 10;
    else if (ImGui::GetIO().KeyCtrl) actual = std::max(1, step / 10);
    const int next = std::clamp(*v + static_cast<int>(wheel) * actual, lo, hi);
    if (next == *v) return false;
    *v = next;
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
// Preset directory scanning

std::vector<fs::path> list_presets(const fs::path& dir) {
    std::vector<fs::path> out;
    std::error_code ec;
    if (!fs::exists(dir, ec)) return out;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            out.push_back(entry.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

void refresh_preset_lists(AppState& state) {
    state.bundled_presets = list_presets("presets");
    state.user_presets = list_presets(caustic::user_preset_dir());
}

// ---------------------------------------------------------------------------
// UI panels

void render_param_panel(AppState& state) {
    auto& p = state.preset;
    ImGui::Begin("Parameters");

    int gen_idx = static_cast<int>(p.generator.type);
    if (ImGui::Combo("Generator", &gen_idx, kGeneratorNames, IM_ARRAYSIZE(kGeneratorNames))) {
        p.generator.type = static_cast<caustic::GeneratorType>(gen_idx);
        state.dirty = true;
    }

    ImGui::Separator();

    switch (p.generator.type) {
        case caustic::GeneratorType::ModularChord:
            if (slider_int_w("N", &p.generator.chord.N, 3, 1000)) state.dirty = true;
            if (slider_double_w("k", &p.generator.chord.k, 0.0, 100.0, 0.01)) state.dirty = true;
            break;
        case caustic::GeneratorType::Hypotrochoid: {
            int Ri = static_cast<int>(std::lround(p.generator.hypo.R));
            int ri = static_cast<int>(std::lround(p.generator.hypo.r));
            if (slider_int_w("R", &Ri, 2, 10)) { p.generator.hypo.R = static_cast<double>(Ri); state.dirty = true; }
            const int r_max = Ri - 1;
            if (ri > r_max) { ri = r_max; p.generator.hypo.r = static_cast<double>(ri); state.dirty = true; }
            if (slider_int_w("r", &ri, 1, r_max)) { p.generator.hypo.r = static_cast<double>(ri); state.dirty = true; }
            if (slider_double_w("d", &p.generator.hypo.d, 0.0, 2.0 * p.generator.hypo.R, 0.01)) state.dirty = true;
            if (slider_int_w("samples", &p.generator.hypo.samples, 100, 100000)) state.dirty = true;
            break;
        }
        case caustic::GeneratorType::Epitrochoid: {
            int Ri = static_cast<int>(std::lround(p.generator.epi.R));
            int ri = static_cast<int>(std::lround(p.generator.epi.r));
            if (slider_int_w("R", &Ri, 1, 10)) { p.generator.epi.R = static_cast<double>(Ri); state.dirty = true; }
            if (slider_int_w("r", &ri, 1, 10)) { p.generator.epi.r = static_cast<double>(ri); state.dirty = true; }
            if (slider_double_w("d", &p.generator.epi.d, 0.0, 3.0 * p.generator.epi.R, 0.01)) state.dirty = true;
            if (slider_int_w("samples", &p.generator.epi.samples, 100, 100000)) state.dirty = true;
            break;
        }
        case caustic::GeneratorType::Lissajous: {
            int ai = static_cast<int>(std::lround(p.generator.liss.a));
            int bi = static_cast<int>(std::lround(p.generator.liss.b));
            if (slider_double_w("A", &p.generator.liss.A, 0.1, 5.0, 0.01)) state.dirty = true;
            if (slider_double_w("B", &p.generator.liss.B, 0.1, 5.0, 0.01)) state.dirty = true;
            if (slider_int_w("a", &ai, 1, 50)) { p.generator.liss.a = static_cast<double>(ai); state.dirty = true; }
            if (slider_int_w("b", &bi, 1, 50)) { p.generator.liss.b = static_cast<double>(bi); state.dirty = true; }
            if (slider_double_w("phi", &p.generator.liss.phi, 0.0, 2.0 * std::numbers::pi, 0.01)) state.dirty = true;
            if (slider_int_w("samples", &p.generator.liss.samples, 100, 100000)) state.dirty = true;
            break;
        }
    }

    ImGui::Separator();
    ImGui::TextDisabled("Ctrl+click any slider to type an exact value (or right-click → Input)");
    ImGui::TextDisabled("scroll wheel on slider: ± step   Shift: ×10   Ctrl: ×0.1");
    ImGui::TextDisabled("keys 1–4: switch generator   F or 0: reset camera   F11: fullscreen");
    ImGui::TextDisabled("middle drag: pan   scroll on canvas: zoom");

    ImGui::End();
}

void render_style_panel(AppState& state) {
    caustic::StyleSpec& s = state.preset.style;
    ImGui::Begin("Style");

    int cm = static_cast<int>(s.colormap_type);
    if (ImGui::Combo("Color map", &cm, kColorMapNames, IM_ARRAYSIZE(kColorMapNames))) {
        s.colormap_type = static_cast<caustic::ColorMapType>(cm);
        state.dirty = true;
    }

    switch (s.colormap_type) {
        case caustic::ColorMapType::Solid:
            if (color_edit_double("color", &s.solid_color)) state.dirty = true;
            break;
        case caustic::ColorMapType::LinearGradient:
            if (color_edit_double("start", &s.gradient_start)) state.dirty = true;
            if (color_edit_double("end",   &s.gradient_end))   state.dirty = true;
            break;
        case caustic::ColorMapType::HsvSweep:
            if (slider_double_w("hue start",  &s.hue_start,      0.0, 360.0, 1.0, "%.0f")) state.dirty = true;
            if (slider_double_w("hue end",    &s.hue_end,        0.0, 360.0, 1.0, "%.0f")) state.dirty = true;
            if (slider_double_w("saturation", &s.hsv_saturation, 0.0,   1.0, 0.01))         state.dirty = true;
            if (slider_double_w("value",      &s.hsv_value,      0.0,   1.0, 0.01))         state.dirty = true;
            break;
        case caustic::ColorMapType::Diverging:
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

    ImGui::Separator();
    ImGui::TextDisabled("click any color square for hex/RGB/HSV input");

    ImGui::End();
}

void render_preset_panel(AppState& state) {
    ImGui::Begin("Presets");

    // Save section
    ImGui::Text("Save (to %s)", caustic::user_preset_dir().string().c_str());
    ImGui::PushItemWidth(180);
    ImGui::InputText("name", state.save_name_buf, sizeof(state.save_name_buf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Save")) {
        try {
            const std::string name = state.save_name_buf;
            const fs::path path = caustic::user_preset_dir() / (name + ".json");
            state.preset.name = name;
            caustic::save_preset(path, state.preset);
            state.status_message = "saved " + path.filename().string();
            refresh_preset_lists(state);
        } catch (const std::exception& e) {
            state.status_message = std::string("save failed: ") + e.what();
        }
    }

    if (!state.status_message.empty()) {
        ImGui::TextDisabled("%s", state.status_message.c_str());
    }

    ImGui::Separator();

    auto try_load = [&](const fs::path& path) {
        try {
            state.preset = caustic::load_preset(path);
            std::strncpy(state.save_name_buf, state.preset.name.c_str(), sizeof(state.save_name_buf) - 1);
            state.save_name_buf[sizeof(state.save_name_buf) - 1] = '\0';
            state.status_message = "loaded " + path.filename().string();
            state.dirty = true;
        } catch (const std::exception& e) {
            state.status_message = std::string("load failed: ") + e.what();
        }
    };

    auto render_list = [&](const char* heading, const std::vector<fs::path>& paths) {
        if (ImGui::CollapsingHeader(heading, ImGuiTreeNodeFlags_DefaultOpen)) {
            if (paths.empty()) {
                ImGui::TextDisabled("  (none)");
            }
            for (const auto& path : paths) {
                ImGui::PushID(path.string().c_str());
                if (ImGui::Selectable(path.stem().string().c_str())) {
                    try_load(path);
                }
                ImGui::PopID();
            }
        }
    };

    render_list("Bundled", state.bundled_presets);
    render_list("User", state.user_presets);

    if (ImGui::Button("Refresh")) refresh_preset_lists(state);

    ImGui::Separator();
    ImGui::Text("Export SVG (to %s)", user_export_dir().string().c_str());
    ImGui::PushItemWidth(180);
    ImGui::InputText("filename", state.export_name_buf, sizeof(state.export_name_buf));
    ImGui::PopItemWidth();
    ImGui::SliderInt("size", &state.export_size, 256, 4096);
    ImGui::Checkbox("plotter mode (single colour, no opacity, sorted)", &state.export_plotter_mode);
    if (ImGui::Button("Export SVG")) {
        try {
            const fs::path path = user_export_dir() / (std::string(state.export_name_buf) + ".svg");
            caustic::SvgOptions opts;
            opts.width = static_cast<double>(state.export_size);
            opts.height = static_cast<double>(state.export_size);
            opts.plotter_mode = state.export_plotter_mode;
            // Export always runs at full quality — see ARCHITECTURE.md §5.4.
            caustic::write_svg(path,
                               build_geometry(state.preset.generator, /*coarse=*/false),
                               build_style(state.preset.style),
                               opts);
            state.status_message = "exported " + path.filename().string();
        } catch (const std::exception& e) {
            state.status_message = std::string("export failed: ") + e.what();
        }
    }

    ImGui::End();
}

}  // namespace

int main() {
    constexpr int kInitialWidth = 1280;
    constexpr int kInitialHeight = 800;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(kInitialWidth, kInitialHeight, "Caustic");
    SetWindowMinSize(640, 400);
    SetTargetFPS(60);
    rlImGuiSetup(true);

    caustic::RaylibRenderer renderer(kInitialWidth, kInitialHeight);
    AppState state;
    refresh_preset_lists(state);

    while (!WindowShouldClose()) {
        const ImGuiIO& io = ImGui::GetIO();

        if (IsWindowResized()) {
            renderer.resize(GetScreenWidth(), GetScreenHeight());
            state.dirty = true;
        }

        if (!io.WantCaptureMouse) {
            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
                const Vector2 d = GetMouseDelta();
                if (d.x != 0.0f || d.y != 0.0f) {
                    state.preset.camera.pan_x_px += d.x;
                    state.preset.camera.pan_y_px += d.y;
                    state.dirty = true;
                }
            }
            const float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                const Vector2 m = GetMousePosition();
                const double cx = GetScreenWidth() / 2.0;
                const double cy = GetScreenHeight() / 2.0;
                const double factor = (wheel > 0.0f) ? 1.1 : (1.0 / 1.1);
                const double new_zoom = std::clamp(state.preset.camera.zoom * factor, 0.1, 100.0);
                const double effective = new_zoom / state.preset.camera.zoom;
                state.preset.camera.pan_x_px = (m.x - cx) * (1.0 - effective) + state.preset.camera.pan_x_px * effective;
                state.preset.camera.pan_y_px = (m.y - cy) * (1.0 - effective) + state.preset.camera.pan_y_px * effective;
                state.preset.camera.zoom = new_zoom;
                state.dirty = true;
            }
        }

        if (!io.WantCaptureKeyboard) {
            const auto prev = state.preset.generator.type;
            if (IsKeyPressed(KEY_ONE))   state.preset.generator.type = caustic::GeneratorType::ModularChord;
            if (IsKeyPressed(KEY_TWO))   state.preset.generator.type = caustic::GeneratorType::Hypotrochoid;
            if (IsKeyPressed(KEY_THREE)) state.preset.generator.type = caustic::GeneratorType::Epitrochoid;
            if (IsKeyPressed(KEY_FOUR))  state.preset.generator.type = caustic::GeneratorType::Lissajous;
            if (state.preset.generator.type != prev) state.dirty = true;

            if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_ZERO)) {
                state.preset.camera = caustic::CameraState{};
                state.dirty = true;
            }

            if (IsKeyPressed(KEY_F11)) {
                ToggleBorderlessWindowed();
                // The resize will be picked up next frame by IsWindowResized().
            }
        }

        const bool dragging = state.any_active_last_frame;
        const bool just_released = state.any_active_two_frames_ago && !dragging;
        if (state.dirty || (just_released && state.last_render_was_coarse)) {
            const bool coarse = dragging;
            renderer.redraw(build_geometry(state.preset.generator, coarse),
                            build_style(state.preset.style),
                            state.preset.camera);
            state.dirty = false;
            state.last_render_was_coarse = coarse;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        renderer.blit_to_screen();

        rlImGuiBegin();
        render_param_panel(state);
        render_style_panel(state);
        render_preset_panel(state);
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
