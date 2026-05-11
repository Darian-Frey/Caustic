#include <algorithm>
#include <cmath>
#include <cstring>
#include <exception>
#include <filesystem>
#include <numbers>
#include <string>
#include <vector>

#include <caustic/animation.hpp>
#include <caustic/array_tools.hpp>
#include <caustic/colormap.hpp>
#include <caustic/envelope.hpp>
#include <caustic/geometry_buffer.hpp>
#include <caustic/geometry_factory.hpp>
#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>
#include <caustic/scene_render.hpp>
#include <caustic/style.hpp>
#include <caustic/style_factory.hpp>

#include "raylib_renderer.hpp"
#include "svg_renderer.hpp"

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>

namespace fs = std::filesystem;

namespace {

const char* const kGeneratorNames[] = {
    "modular chord", "hypotrochoid", "epitrochoid", "lissajous",
    "rose", "superformula", "phyllotaxis",
    "polygon chord", "linear envelope",
};

const char* const kIndexerNames[] = {
    "chord index", "chord length", "angle", "curve t",
};

const char* const kColorMapNames[] = {
    "solid", "linear gradient", "hsv sweep", "diverging",
};

struct AppState {
    caustic::Preset preset;
    int current_layer_idx = 0;  // index into preset.scene.layers being edited by the UI

    caustic::anim::AnimationSpec animation;
    char bake_name_buf[64] = "animation";

    char save_name_buf[64] = "untitled";
    char export_name_buf[64] = "export";
    bool export_plotter_mode = false;
    int export_size = 1024;
    std::string status_message;     // shown briefly after save/load/export/bake
    std::vector<fs::path> bundled_presets;
    std::vector<fs::path> user_presets;

    bool dirty = true;
    bool any_active_last_frame = false;
    bool any_active_two_frames_ago = false;
    bool last_render_was_coarse = false;

    // Ensure preset has at least one layer; clamp current_layer_idx within bounds.
    caustic::Layer& current_layer() {
        if (preset.scene.layers.empty()) preset.scene.layers.emplace_back();
        if (current_layer_idx < 0 ||
            current_layer_idx >= static_cast<int>(preset.scene.layers.size())) {
            current_layer_idx = 0;
        }
        return preset.scene.layers[current_layer_idx];
    }
};

fs::path user_export_dir() {
    return caustic::user_preset_dir().parent_path() / "exports";
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
    auto& p = state.current_layer();  // edit the layer the user has selected
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
        case caustic::GeneratorType::Rose:
            // n, d are integers for closure (r = cos(n·θ/d) closes at 2π·d).
            if (slider_int_w("n",       &p.generator.rose.n,       1, 20)) state.dirty = true;
            if (slider_int_w("d",       &p.generator.rose.d,       1, 10)) state.dirty = true;
            if (slider_int_w("samples", &p.generator.rose.samples, 100, 100000)) state.dirty = true;
            break;
        case caustic::GeneratorType::Superformula:
            if (slider_double_w("m",       &p.generator.supf.m,        0.0,  20.0,  0.1, "%.2f")) state.dirty = true;
            if (slider_double_w("n1",      &p.generator.supf.n1,       0.1, 100.0,  0.1, "%.2f")) state.dirty = true;
            if (slider_double_w("n2",      &p.generator.supf.n2,       0.0, 100.0,  0.1, "%.2f")) state.dirty = true;
            if (slider_double_w("n3",      &p.generator.supf.n3,       0.0, 100.0,  0.1, "%.2f")) state.dirty = true;
            if (slider_double_w("a",       &p.generator.supf.a,        0.1,   5.0,  0.01))         state.dirty = true;
            if (slider_double_w("b",       &p.generator.supf.b,        0.1,   5.0,  0.01))         state.dirty = true;
            if (slider_int_w("samples",    &p.generator.supf.samples,  100, 100000))               state.dirty = true;
            break;
        case caustic::GeneratorType::Phyllotaxis: {
            // Phyllotaxis has a huge chaotic parameter space; almost all of it
            // is mathematically valid but visually noisy. Snap buttons surface
            // the aesthetically stable points (golden angle, n-spoke
            // 2π/n divisions, small-integer k) so the user can find them
            // without remembering exact values.
            if (slider_int_w("N",        &p.generator.phyl.N,     10, 5000)) state.dirty = true;

            if (slider_double_w("alpha", &p.generator.phyl.alpha, 0.0, 2.0 * std::numbers::pi, 0.0001, "%.5f")) state.dirty = true;
            const auto snap_alpha = [&](double a) {
                p.generator.phyl.alpha = a;
                state.dirty = true;
            };
            ImGui::Text("α snap:");
            ImGui::SameLine(); if (ImGui::SmallButton("golden")) snap_alpha(2.39996322972865332);
            ImGui::SameLine(); if (ImGui::SmallButton("2π/3"))   snap_alpha(2.0 * std::numbers::pi / 3.0);
            ImGui::SameLine(); if (ImGui::SmallButton("2π/5"))   snap_alpha(2.0 * std::numbers::pi / 5.0);
            ImGui::SameLine(); if (ImGui::SmallButton("2π/7"))   snap_alpha(2.0 * std::numbers::pi / 7.0);
            ImGui::SameLine(); if (ImGui::SmallButton("π/2"))    snap_alpha(std::numbers::pi / 2.0);

            if (slider_double_w("k", &p.generator.phyl.k, 0.0, 100.0, 0.01)) state.dirty = true;
            const auto snap_k = [&](double v) {
                p.generator.phyl.k = v;
                state.dirty = true;
            };
            ImGui::Text("k snap:");
            ImGui::SameLine(); if (ImGui::SmallButton("2"))  snap_k(2.0);
            ImGui::SameLine(); if (ImGui::SmallButton("3"))  snap_k(3.0);
            ImGui::SameLine(); if (ImGui::SmallButton("5"))  snap_k(5.0);
            ImGui::SameLine(); if (ImGui::SmallButton("7"))  snap_k(7.0);
            ImGui::SameLine(); if (ImGui::SmallButton("11")) snap_k(11.0);
            break;
        }
        case caustic::GeneratorType::PolygonChord:
            if (slider_int_w("n sides", &p.generator.poly.n_sides, 3, 12)) state.dirty = true;
            if (slider_int_w("N",       &p.generator.poly.N,       3, 2000)) state.dirty = true;
            if (slider_double_w("k",    &p.generator.poly.k,       0.0, 100.0, 0.01)) state.dirty = true;
            if (slider_double_w("rotation", &p.generator.poly.rotation_rad,
                                -std::numbers::pi, std::numbers::pi, 0.01)) state.dirty = true;
            break;
        case caustic::GeneratorType::LinearEnvelope: {
            // Line endpoints exposed as SliderFloat2 pairs so each line is one
            // widget. A future polish would let you drag the endpoints on the
            // canvas itself; for now keyboard-friendly sliders.
            auto edit_endpoint = [&](const char* label, caustic::Vec2& v) {
                float xy[2] = {static_cast<float>(v.x), static_cast<float>(v.y)};
                if (ImGui::SliderFloat2(label, xy, -3.0f, 3.0f, "%.3f")) {
                    v = {xy[0], xy[1]};
                    state.dirty = true;
                }
            };
            edit_endpoint("a start", p.generator.lenv.a_start);
            edit_endpoint("a end",   p.generator.lenv.a_end);
            edit_endpoint("b start", p.generator.lenv.b_start);
            edit_endpoint("b end",   p.generator.lenv.b_end);
            if (slider_int_w("N",    &p.generator.lenv.N, 2, 500)) state.dirty = true;
            if (slider_double_w("k", &p.generator.lenv.k, -100.0, 100.0, 0.1)) state.dirty = true;
            break;
        }
    }

    if (ImGui::Button("Reset generator params")) {
        // Reset only the active generator's params to their struct defaults,
        // leaving other generators' configs alone. Useful for escaping a
        // chaotic-looking corner of parameter space (e.g. phyllotaxis with
        // non-golden α and large k).
        switch (p.generator.type) {
            case caustic::GeneratorType::ModularChord: p.generator.chord = caustic::ModularChordParams{}; break;
            case caustic::GeneratorType::Hypotrochoid: p.generator.hypo  = caustic::HypotrochoidParams{}; break;
            case caustic::GeneratorType::Epitrochoid:  p.generator.epi   = caustic::EpitrochoidParams{};  break;
            case caustic::GeneratorType::Lissajous:    p.generator.liss  = caustic::LissajousParams{};    break;
            case caustic::GeneratorType::Rose:           p.generator.rose = caustic::RoseParams{};           break;
            case caustic::GeneratorType::Superformula:   p.generator.supf = caustic::SuperformulaParams{};   break;
            case caustic::GeneratorType::Phyllotaxis:    p.generator.phyl = caustic::PhyllotaxisParams{};    break;
            case caustic::GeneratorType::PolygonChord:   p.generator.poly = caustic::PolygonChordParams{};   break;
            case caustic::GeneratorType::LinearEnvelope: p.generator.lenv = caustic::LinearEnvelopeParams{}; break;
        }
        state.dirty = true;
    }

    ImGui::Separator();
    ImGui::TextDisabled("Ctrl+click any slider to type an exact value (or right-click → Input)");
    ImGui::TextDisabled("scroll wheel on slider: ± step   Shift: ×10   Ctrl: ×0.1");
    ImGui::TextDisabled("keys 1–4: switch generator   F or 0: reset camera   F11: fullscreen");
    ImGui::TextDisabled("middle drag: pan   scroll on canvas: zoom");

    ImGui::End();
}

void render_style_panel(AppState& state) {
    caustic::StyleSpec& s = state.current_layer().style;
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

    if (color_edit_double("background", &state.preset.scene.background)) state.dirty = true;
    if (ImGui::Checkbox("cyclic (closed-curve continuity)", &s.cyclic)) state.dirty = true;

    if (ImGui::Button("Reset style")) {
        // Resets the active layer's StyleSpec only. Scene-level background is
        // not touched (it's shared across all layers).
        state.current_layer().style = caustic::StyleSpec{};
        state.dirty = true;
    }

    ImGui::Separator();
    ImGui::TextDisabled("click any color square for hex/RGB/HSV input");

    ImGui::End();
}

void render_layers_panel(AppState& state) {
    ImGui::Begin("Layers");

    // ----- Layer list -----
    auto& layers = state.preset.scene.layers;
    if (layers.empty()) layers.emplace_back();
    if (state.current_layer_idx >= static_cast<int>(layers.size())) state.current_layer_idx = 0;
    if (state.current_layer_idx < 0) state.current_layer_idx = 0;

    if (ImGui::BeginListBox("##layers", ImVec2(-FLT_MIN, 6 * ImGui::GetTextLineHeightWithSpacing()))) {
        for (int i = 0; i < static_cast<int>(layers.size()); ++i) {
            ImGui::PushID(i);
            bool visible = layers[i].visible;
            if (ImGui::Checkbox("##visible", &visible)) {
                layers[i].visible = visible;
                state.dirty = true;
            }
            ImGui::SameLine();
            const std::string label = layers[i].name.empty()
                ? ("layer " + std::to_string(i))
                : layers[i].name;
            const bool selected = (i == state.current_layer_idx);
            if (ImGui::Selectable(label.c_str(), selected)) {
                state.current_layer_idx = i;
            }
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }

    // ----- Layer actions -----
    if (ImGui::Button("Add")) {
        caustic::Layer l;
        l.name = "layer " + std::to_string(layers.size());
        layers.push_back(std::move(l));
        state.current_layer_idx = static_cast<int>(layers.size()) - 1;
        state.dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Duplicate") && state.current_layer_idx >= 0) {
        caustic::Layer copy = layers[state.current_layer_idx];
        copy.name += " (copy)";
        layers.insert(layers.begin() + state.current_layer_idx + 1, std::move(copy));
        state.current_layer_idx += 1;
        state.dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Remove") && layers.size() > 1) {
        layers.erase(layers.begin() + state.current_layer_idx);
        if (state.current_layer_idx >= static_cast<int>(layers.size())) {
            state.current_layer_idx = static_cast<int>(layers.size()) - 1;
        }
        state.dirty = true;
    }
    if (ImGui::Button("Move up") && state.current_layer_idx > 0) {
        std::swap(layers[state.current_layer_idx], layers[state.current_layer_idx - 1]);
        state.current_layer_idx -= 1;
        state.dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Move down") &&
        state.current_layer_idx + 1 < static_cast<int>(layers.size())) {
        std::swap(layers[state.current_layer_idx], layers[state.current_layer_idx + 1]);
        state.current_layer_idx += 1;
        state.dirty = true;
    }

    // ----- Transform sliders for the current layer -----
    ImGui::Separator();
    ImGui::Text("Transform");
    auto& t = state.current_layer().transform;
    char name_buf[64];
    std::strncpy(name_buf, state.current_layer().name.c_str(), sizeof(name_buf) - 1);
    name_buf[sizeof(name_buf) - 1] = '\0';
    if (ImGui::InputText("name", name_buf, sizeof(name_buf))) {
        state.current_layer().name = name_buf;
    }
    if (slider_double_w("tx",    &t.translate.x,  -10.0, 10.0, 0.01)) state.dirty = true;
    if (slider_double_w("ty",    &t.translate.y,  -10.0, 10.0, 0.01)) state.dirty = true;
    if (slider_double_w("rot",   &t.rotate_rad,   -2.0 * std::numbers::pi, 2.0 * std::numbers::pi, 0.01)) state.dirty = true;
    if (slider_double_w("scale", &t.scale,         0.05, 5.0, 0.01))  state.dirty = true;
    if (ImGui::Checkbox("mirror x", &t.mirror_x)) state.dirty = true;
    ImGui::SameLine();
    if (ImGui::Checkbox("mirror y", &t.mirror_y)) state.dirty = true;
    if (ImGui::Button("Reset transform")) {
        t = {};
        state.dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset layer")) {
        // Wipe generator + style + transform back to defaults; keep the layer's
        // name and visibility so the user's place in the layer list is preserved.
        auto& L = state.current_layer();
        L.generator = caustic::GeneratorSpec{};
        L.style     = caustic::StyleSpec{};
        L.transform = caustic::LayerTransform{};
        state.dirty = true;
    }

    // ----- Array tools -----
    ImGui::Separator();
    ImGui::Text("Array tools — Apply replaces the selected layer with N derived copies");
    static int array_n = 6;
    static int grid_rows = 2;
    static int grid_cols = 4;
    static float grid_spacing = 1.0f;
    static int mirror_axis_idx = 1;  // Y by default (left-right mirror)

    ImGui::SliderInt("N (rotational)", &array_n, 2, 24);
    if (ImGui::Button("Apply rotational array")) {
        auto derived = caustic::rotational_array(state.current_layer(), array_n);
        const int idx = state.current_layer_idx;
        layers.erase(layers.begin() + idx);
        layers.insert(layers.begin() + idx, derived.begin(), derived.end());
        state.dirty = true;
    }
    ImGui::SliderInt("rows", &grid_rows, 1, 10);
    ImGui::SliderInt("cols", &grid_cols, 1, 10);
    ImGui::SliderFloat("spacing", &grid_spacing, 0.1f, 5.0f);
    if (ImGui::Button("Apply grid tile")) {
        auto derived = caustic::grid_tile(state.current_layer(), grid_rows, grid_cols,
                                          {grid_spacing, grid_spacing});
        const int idx = state.current_layer_idx;
        layers.erase(layers.begin() + idx);
        layers.insert(layers.begin() + idx, derived.begin(), derived.end());
        state.dirty = true;
    }
    const char* axis_names[] = {"X axis", "Y axis"};
    ImGui::Combo("mirror axis", &mirror_axis_idx, axis_names, IM_ARRAYSIZE(axis_names));
    if (ImGui::Button("Apply mirror reflect")) {
        const caustic::MirrorAxis axis = (mirror_axis_idx == 0) ? caustic::MirrorAxis::X
                                                                : caustic::MirrorAxis::Y;
        auto derived = caustic::mirror_reflect(state.current_layer(), axis);
        const int idx = state.current_layer_idx;
        layers.erase(layers.begin() + idx);
        layers.insert(layers.begin() + idx, derived.begin(), derived.end());
        state.dirty = true;
    }

    ImGui::End();
}

// ---------------------------------------------------------------------------
// Animation panel

fs::path user_animation_dir() {
    return caustic::user_preset_dir().parent_path() / "animations";
}

void render_animation_panel(AppState& state) {
    auto& anim = state.animation;
    ImGui::Begin("Animation");

    // Target picker. Listed in the same order as caustic::anim::Target so
    // the index round-trips cleanly.
    static const char* const kTargetNames[] = {
        "(none — animation off)",
        "modular chord: k",
        "hypotrochoid: d",
        "epitrochoid: d",
        "lissajous: phi",
        "lissajous: A",
        "lissajous: B",
        "phyllotaxis: alpha",
        "phyllotaxis: k",
        "polygon chord: k",
        "polygon chord: rotation",
        "layer: rotate",
        "layer: scale",
        "layer: translate x",
        "layer: translate y",
        "camera: zoom",
    };
    int target_idx = static_cast<int>(anim.target);
    if (ImGui::Combo("target", &target_idx, kTargetNames, IM_ARRAYSIZE(kTargetNames))) {
        anim.target = static_cast<caustic::anim::Target>(target_idx);
        state.dirty = true;
    }

    // Envelope type
    int env_idx = static_cast<int>(anim.envelope.index());
    static const char* const kEnvNames[] = {"static", "linear", "sine"};
    if (ImGui::Combo("envelope", &env_idx, kEnvNames, IM_ARRAYSIZE(kEnvNames))) {
        switch (env_idx) {
            case 0: anim.envelope = caustic::anim::Static{};  break;
            case 1: anim.envelope = caustic::anim::Linear{};  break;
            case 2: anim.envelope = caustic::anim::Sine{};    break;
        }
        state.dirty = true;
    }

    // Envelope params (depend on type).
    if (auto* s = std::get_if<caustic::anim::Static>(&anim.envelope)) {
        if (slider_double_w("value", &s->value, -100.0, 100.0, 0.01)) state.dirty = true;
    } else if (auto* l = std::get_if<caustic::anim::Linear>(&anim.envelope)) {
        if (slider_double_w("from", &l->v0, -100.0, 100.0, 0.01)) state.dirty = true;
        if (slider_double_w("to",   &l->v1, -100.0, 100.0, 0.01)) state.dirty = true;
    } else if (auto* w = std::get_if<caustic::anim::Sine>(&anim.envelope)) {
        if (slider_double_w("amplitude", &w->amplitude, -100.0, 100.0, 0.01)) state.dirty = true;
        if (slider_double_w("frequency", &w->frequency,    0.0,  10.0, 0.01)) state.dirty = true;
        if (slider_double_w("phase",     &w->phase,       -std::numbers::pi, std::numbers::pi, 0.01)) state.dirty = true;
        if (slider_double_w("offset",    &w->offset,    -100.0, 100.0, 0.01)) state.dirty = true;
    }

    ImGui::Separator();

    if (slider_double_w("duration (s)", &anim.duration_sec, 0.1, 60.0, 0.1, "%.2f")) {}

    if (ImGui::Button(anim.playing ? "Pause" : "Play")) {
        anim.playing = !anim.playing;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset to t=0")) {
        anim.current_t = 0.0;
        state.dirty = true;
    }

    float t_f = static_cast<float>(anim.current_t);
    if (ImGui::SliderFloat("time", &t_f, 0.0f, 1.0f, "%.3f")) {
        anim.current_t = t_f;
        anim.playing = false;  // scrubbing pauses playback
        state.dirty = true;
    }

    ImGui::Separator();
    ImGui::Text("Bake to SVG sequence (output dir: %s)", user_animation_dir().string().c_str());
    if (slider_int_w("frames", &anim.bake_frames, 2, 600)) {}
    ImGui::PushItemWidth(180);
    ImGui::InputText("name prefix", state.bake_name_buf, sizeof(state.bake_name_buf));
    ImGui::PopItemWidth();
    if (ImGui::Button("Bake SVG sequence")) {
        try {
            const fs::path dir = user_animation_dir();
            fs::create_directories(dir);
            caustic::SvgOptions opts;
            opts.width = static_cast<double>(state.export_size);
            opts.height = static_cast<double>(state.export_size);
            // Iterate frames, writing a numbered SVG per frame. Animation
            // mutates a copy of the preset so the user's live state is
            // preserved while baking.
            const int frames = std::max(2, anim.bake_frames);
            for (int i = 0; i < frames; ++i) {
                const double t = static_cast<double>(i) / static_cast<double>(frames - 1);
                caustic::Preset frame = state.preset;
                if (anim.target != caustic::anim::Target::None) {
                    const double value = caustic::anim::evaluate(anim.envelope, t);
                    caustic::anim::write_target(anim.target, value, frame, state.current_layer_idx);
                }
                char buf[80];
                std::snprintf(buf, sizeof(buf), "%s_%04d.svg", state.bake_name_buf, i);
                caustic::write_svg(dir / buf,
                                   caustic::build_renderables(frame.scene, /*coarse=*/false),
                                   frame.scene.background,
                                   opts);
            }
            state.status_message = "baked " + std::to_string(frames) + " frames to " + dir.string();
        } catch (const std::exception& e) {
            state.status_message = std::string("bake failed: ") + e.what();
        }
    }

    if (!state.status_message.empty()) {
        ImGui::TextDisabled("%s", state.status_message.c_str());
    }

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
                               caustic::build_renderables(state.preset.scene, /*coarse=*/false),
                               state.preset.scene.background,
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

    // Inner scope so `renderer` and `state` destruct BEFORE rlImGuiShutdown()
    // and CloseWindow() tear down the GL context. RaylibRenderer's destructor
    // calls UnloadRenderTexture, which segfaults if the GL context is already
    // gone — that was the source of the on-exit segfault.
    {
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
            auto& cur_gen = state.current_layer().generator;
            const auto prev = cur_gen.type;
            if (IsKeyPressed(KEY_ONE))   cur_gen.type = caustic::GeneratorType::ModularChord;
            if (IsKeyPressed(KEY_TWO))   cur_gen.type = caustic::GeneratorType::Hypotrochoid;
            if (IsKeyPressed(KEY_THREE)) cur_gen.type = caustic::GeneratorType::Epitrochoid;
            if (IsKeyPressed(KEY_FOUR))  cur_gen.type = caustic::GeneratorType::Lissajous;
            if (cur_gen.type != prev) state.dirty = true;

            if (IsKeyPressed(KEY_F) || IsKeyPressed(KEY_ZERO)) {
                state.preset.camera = caustic::CameraState{};
                state.dirty = true;
            }

            if (IsKeyPressed(KEY_F11)) {
                ToggleBorderlessWindowed();
                // The resize will be picked up next frame by IsWindowResized().
            }
        }

        // Animation: tick when playing, evaluate envelope, write target.
        // current_t change always dirties the canvas. While a target is set,
        // any scrub or play advances the geometry — pausing freezes it.
        if (state.animation.target != caustic::anim::Target::None) {
            const bool changed = caustic::anim::tick(state.animation, GetFrameTime());
            // Always apply the current_t (so manual scrubs take effect too).
            const double value = caustic::anim::evaluate(state.animation.envelope,
                                                         state.animation.current_t);
            caustic::anim::write_target(state.animation.target, value,
                                        state.preset, state.current_layer_idx);
            if (changed) state.dirty = true;
        }

        const bool dragging = state.any_active_last_frame;
        const bool just_released = state.any_active_two_frames_ago && !dragging;
        if (state.dirty || (just_released && state.last_render_was_coarse)) {
            const bool coarse = dragging;
            try {
                renderer.redraw(caustic::build_renderables(state.preset.scene, coarse),
                                state.preset.scene.background,
                                state.preset.camera);
                state.dirty = false;
                state.last_render_was_coarse = coarse;
            } catch (const std::exception& e) {
                // Don't loop forever on a bad spec — clear dirty and surface the
                // error to the user instead of aborting.
                state.dirty = false;
                state.status_message = std::string("render failed: ") + e.what();
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);
        renderer.blit_to_screen();

        rlImGuiBegin();
        render_param_panel(state);
        render_style_panel(state);
        render_layers_panel(state);
        render_animation_panel(state);
        render_preset_panel(state);
        const bool active_now = ImGui::IsAnyItemActive();
        rlImGuiEnd();

        EndDrawing();

        state.any_active_two_frames_ago = state.any_active_last_frame;
        state.any_active_last_frame = active_now;
    }
    }  // end inner scope — renderer + state destruct here, while GL context is still live

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
