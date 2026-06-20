#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <caustic/animation.hpp>
#include <caustic/array_tools.hpp>
#include <caustic/colormap.hpp>
#include <caustic/envelope.hpp>
#include <caustic/geometry_buffer.hpp>
#include <caustic/geometry_factory.hpp>
#include <caustic/preset.hpp>
#include <caustic/preset_io.hpp>
#include <caustic/preset_url.hpp>
#include <caustic/randomize.hpp>
#include <caustic/scene_render.hpp>
#include <caustic/style.hpp>
#include <caustic/style_factory.hpp>

#include "raylib_renderer.hpp"
#include "svg_renderer.hpp"

#include <imgui.h>
#include <raylib.h>
#include <rlImGui.h>
#include <tinyfiledialogs.h>

// msf_gif is a single-header GIF encoder. Define MSF_GIF_IMPL in exactly
// one translation unit (this one) and include normally everywhere else.
#define MSF_GIF_IMPL
#include <msf_gif.h>

namespace fs = std::filesystem;

// File-dialog helpers. tinyfd returns a pointer into a static buffer (or
// nullptr on cancel); we copy to fs::path immediately. Filter patterns are
// shell-glob style, e.g. {"*.json"}.

inline fs::path pick_save_file(const fs::path& default_dir,
                               const std::string& default_name,
                               const char* filter_label,
                               const char* filter_pattern) {
    const fs::path suggested = default_dir / default_name;
    const char* filters[1] = { filter_pattern };
    const char* result = tinyfd_saveFileDialog(
        "Save file", suggested.string().c_str(), 1, filters, filter_label);
    return result ? fs::path(result) : fs::path{};
}

inline fs::path pick_open_file(const fs::path& default_dir,
                               const char* filter_label,
                               const char* filter_pattern) {
    const char* filters[1] = { filter_pattern };
    const char* result = tinyfd_openFileDialog(
        "Open file", default_dir.string().c_str(), 1, filters, filter_label,
        /*aAllowMultipleSelects=*/0);
    return result ? fs::path(result) : fs::path{};
}

inline fs::path pick_folder(const fs::path& default_dir) {
    const char* result = tinyfd_selectFolderDialog(
        "Choose folder", default_dir.string().c_str());
    return result ? fs::path(result) : fs::path{};
}

namespace {

const char* const kGeneratorNames[] = {
    "modular chord", "hypotrochoid", "epitrochoid", "lissajous",
    "rose", "superformula", "phyllotaxis",
    "polygon chord", "linear envelope",
    "clifford", "de jong", "tinkerbell",
    "diamond stack",
    "custom chord",
    "maurer rose", "lissajous chord", "superformula chord",
};

// Nail-editor mode for the CustomChord generator. AddNail = left-click on
// canvas drops a nail at the world coord under the cursor. AddChord = first
// left-click selects a nail, second click on a different nail emits a chord.
// MoveNail = left-click-and-hold on an existing nail drags it to a new
// position; release drops it. Honours snap-to-grid like AddNail.
// RecolourChord = left-click on an existing chord rewrites its start/end
// colours to the panel's active picker values.
// Select = build a multi-item selection: click an item to select it (Shift
// to toggle/add), drag in empty space for a rubber-band rectangle. Delete
// key removes all selected; "Recolour selected" button applies the active
// colours to all selected chords at once.
//
// Right-click is mode-independent: it deletes whatever the cursor is over
// (nail first if both a nail and a chord are in range).
enum class NailEditMode { Off, AddNail, AddChord, MoveNail, RecolourChord, Select };

// Editor-grid mode + state now live in caustic::Preset.editor_grid so they
// round-trip through save/load. See preset.hpp for the EditorGrid struct.

// Draw the editor grid overlay on top of the canvas. Caller passes the
// screen-coord conversion (cx, cy = canvas centre in pixels — these are
// **window-space coordinates** so we share them with raylib calls; pan x/y in
// pixels, scale = pixels-per-world-unit). canvas_x/y/w/h is the canvas
// region in window-space — the grid clips to that rectangle so sidebars
// don't get garnished and the world-extent loop terminates correctly when the
// canvas is narrow.
inline void draw_grid_overlay(caustic::EditorGridMode mode,
                              double spacing, int spokes,
                              double cx, double cy,
                              double pan_x, double pan_y,
                              double scale,
                              int canvas_x, int canvas_y,
                              int canvas_w, int canvas_h) {
    if (spacing < 1e-6) return;
    const ::Color grid_col{80, 110, 130, 90};

    if (mode == caustic::EditorGridMode::Rectangular) {
        const double world_x_min = (canvas_x - cx - pan_x) / scale;
        const double world_x_max = ((canvas_x + canvas_w) - cx - pan_x) / scale;
        const double world_y_min = -(((canvas_y + canvas_h) - cy - pan_y) / scale);
        const double world_y_max = -((canvas_y - cy - pan_y) / scale);
        if ((world_x_max - world_x_min) / spacing > 400.0) return;
        if ((world_y_max - world_y_min) / spacing > 400.0) return;
        const int ix_start = static_cast<int>(std::floor(world_x_min / spacing));
        const int ix_end   = static_cast<int>(std::ceil(world_x_max / spacing));
        for (int ix = ix_start; ix <= ix_end; ++ix) {
            const float sx = static_cast<float>(cx + pan_x + ix * spacing * scale);
            DrawLine(static_cast<int>(sx), canvas_y,
                     static_cast<int>(sx), canvas_y + canvas_h, grid_col);
        }
        const int iy_start = static_cast<int>(std::floor(world_y_min / spacing));
        const int iy_end   = static_cast<int>(std::ceil(world_y_max / spacing));
        for (int iy = iy_start; iy <= iy_end; ++iy) {
            const float sy = static_cast<float>(cy + pan_y - iy * spacing * scale);
            DrawLine(canvas_x, static_cast<int>(sy),
                     canvas_x + canvas_w, static_cast<int>(sy), grid_col);
        }
        return;
    }

    // Polar — concentric rings + N spokes around (cx + pan, cy + pan).
    // Max ring radius is the farthest canvas-corner distance from origin.
    const float ocx = static_cast<float>(cx + pan_x);
    const float ocy = static_cast<float>(cy + pan_y);
    auto pixel_dist = [&](double sx, double sy) {
        const double dx = sx - ocx;
        const double dy = sy - ocy;
        return std::sqrt(dx * dx + dy * dy);
    };
    const double corner_px = std::max(
        std::max(pixel_dist(canvas_x, canvas_y),
                 pixel_dist(canvas_x + canvas_w, canvas_y)),
        std::max(pixel_dist(canvas_x, canvas_y + canvas_h),
                 pixel_dist(canvas_x + canvas_w, canvas_y + canvas_h)));
    const double r_max_world = corner_px / scale;
    if (r_max_world / spacing > 400.0) return;
    const int n_rings = static_cast<int>(std::ceil(r_max_world / spacing));
    for (int i = 1; i <= n_rings; ++i) {
        const float rpx = static_cast<float>(i * spacing * scale);
        DrawCircleLines(static_cast<int>(ocx), static_cast<int>(ocy), rpx, grid_col);
    }
    const int n = std::max(1, spokes);
    const double step = 2.0 * std::numbers::pi / static_cast<double>(n);
    for (int i = 0; i < n; ++i) {
        const double th = step * i;
        const float ex = static_cast<float>(ocx + r_max_world * scale * std::cos(th));
        const float ey = static_cast<float>(ocy - r_max_world * scale * std::sin(th));
        DrawLine(static_cast<int>(ocx), static_cast<int>(ocy),
                 static_cast<int>(ex), static_cast<int>(ey), grid_col);
    }
}

// Snap a world-coord point to the active grid. spacing > 0 required.
inline void snap_world_point(double& x, double& y,
                             caustic::EditorGridMode mode,
                             double spacing, int spokes) {
    if (mode == caustic::EditorGridMode::Rectangular) {
        x = std::round(x / spacing) * spacing;
        y = std::round(y / spacing) * spacing;
        return;
    }
    // Polar
    const double r = std::sqrt(x * x + y * y);
    if (r < 1e-9) { x = 0.0; y = 0.0; return; }
    const double snapped_r = std::round(r / spacing) * spacing;
    const int n = std::max(1, spokes);
    const double step = 2.0 * std::numbers::pi / static_cast<double>(n);
    const double theta = std::atan2(y, x);
    const double snapped_theta = std::round(theta / step) * step;
    x = snapped_r * std::cos(snapped_theta);
    y = snapped_r * std::sin(snapped_theta);
}

// Delete the nail at `idx` from a CustomChordParams. Drops any chord that
// referenced it; re-indexes the remaining chords so indices above `idx` shift
// down by one. Keeps chord_colors / chord_end_colors aligned with chords.
inline void delete_nail_in_custom(caustic::CustomChordParams& c, int idx) {
    if (idx < 0 || idx >= static_cast<int>(c.nails.size())) return;
    c.nails.erase(c.nails.begin() + idx);
    std::vector<std::pair<int, int>> kept_chords;
    std::vector<caustic::Color>      kept_colors;
    std::vector<caustic::Color>      kept_end_colors;
    std::vector<double>              kept_widths;
    std::vector<double>              kept_opacities;
    const bool have_colors     = c.chord_colors.size()     == c.chords.size();
    const bool have_end_colors = c.chord_end_colors.size() == c.chords.size();
    const bool have_widths     = c.chord_widths.size()     == c.chords.size();
    const bool have_opacities  = c.chord_opacities.size()  == c.chords.size();
    for (std::size_t k = 0; k < c.chords.size(); ++k) {
        auto p = c.chords[k];
        if (p.first == idx || p.second == idx) continue;
        if (p.first  > idx) --p.first;
        if (p.second > idx) --p.second;
        kept_chords.push_back(p);
        if (have_colors)     kept_colors.push_back(c.chord_colors[k]);
        if (have_end_colors) kept_end_colors.push_back(c.chord_end_colors[k]);
        if (have_widths)     kept_widths.push_back(c.chord_widths[k]);
        if (have_opacities)  kept_opacities.push_back(c.chord_opacities[k]);
    }
    c.chords = std::move(kept_chords);
    if (have_colors)     c.chord_colors     = std::move(kept_colors);
    if (have_end_colors) c.chord_end_colors = std::move(kept_end_colors);
    if (have_widths)     c.chord_widths     = std::move(kept_widths);
    if (have_opacities)  c.chord_opacities  = std::move(kept_opacities);
}

// Delete the chord at `idx` (and its corresponding per-chord override entries).
inline void delete_chord_in_custom(caustic::CustomChordParams& c, int idx) {
    if (idx < 0 || idx >= static_cast<int>(c.chords.size())) return;
    c.chords.erase(c.chords.begin() + idx);
    if (idx < static_cast<int>(c.chord_colors.size())) {
        c.chord_colors.erase(c.chord_colors.begin() + idx);
    }
    if (idx < static_cast<int>(c.chord_end_colors.size())) {
        c.chord_end_colors.erase(c.chord_end_colors.begin() + idx);
    }
    if (idx < static_cast<int>(c.chord_widths.size())) {
        c.chord_widths.erase(c.chord_widths.begin() + idx);
    }
    if (idx < static_cast<int>(c.chord_opacities.size())) {
        c.chord_opacities.erase(c.chord_opacities.begin() + idx);
    }
}

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
    bool encode_mp4_after_png = true;  // try `ffmpeg` after a PNG bake
    // After a single-file bake (GIF; PNG→mp4 when mp4 encodes), wipe the
    // per-frame intermediates (<prefix>_NNNN.svg / .png / .jpg) in the
    // output folder so the user is left with just the final file. Default
    // true: most users want a single .gif / .mp4, not the frame dump.
    bool clean_bake_intermediates = true;

    char save_name_buf[64] = "untitled";
    char export_name_buf[64] = "export";
    bool export_plotter_mode = false;
    int export_size = 1024;
    // Static-image export format. SVG = vector (resolution-independent + plotter
    // mode); PNG = lossless raster; JPG = lossy raster with no alpha (background
    // is filled with the scene background colour). All write through the same
    // "Export…" button / Ctrl+E shortcut; the native file picker shows a filter
    // matching the chosen format and appends the right extension.
    enum class ExportFormat { Svg, Png, Jpg };
    ExportFormat export_format = ExportFormat::Svg;
    std::string status_message;     // shown briefly after save/load/export/bake
    std::vector<fs::path> bundled_presets;
    std::vector<fs::path> user_presets;

    bool dirty = true;
    bool any_active_last_frame = false;
    bool any_active_two_frames_ago = false;
    bool last_render_was_coarse = false;

    // Custom-chord nail editor state. Only meaningful when the current
    // layer's generator is CustomChord and nail_edit_mode != Off.
    NailEditMode nail_edit_mode = NailEditMode::Off;
    int nail_chord_first = -1;       // first nail clicked in AddChord mode; -1 = none
    int nail_dragging_idx = -1;      // nail being dragged in MoveNail mode; -1 = none

    // Select mode state. Sorted vectors of selected indices; rubber_band_active
    // tracks an in-progress area-select drag started in empty space.
    std::vector<int> selected_nails;
    std::vector<int> selected_chords;
    bool rubber_band_active = false;
    Vector2 rubber_band_start_screen{0.0f, 0.0f};

    // LinearEnvelope canvas-drag editor — when the current layer is
    // LinearEnvelope, left-click on one of the 4 endpoints (a_start, a_end,
    // b_start, b_end) and drag to move it. Index 0..3 maps to that order.
    int lenv_dragging_idx = -1;
    std::vector<caustic::LinearEnvelopeParams> lenv_undo_stack;
    std::vector<caustic::LinearEnvelopeParams> lenv_redo_stack;
    // Set true on left-press when the click didn't grab any editable target
    // (a LinearEnvelope handle, or a CustomChord nail/chord depending on mode).
    // While true, left-click drag pans the canvas — gives left-click drag pan
    // in every layer/mode without breaking the editor's on-target interactions.
    bool left_drag_pan = false;
    // Editor grid state moved into preset.editor_grid so saves persist it.
    bool nail_numbers_visible = true; // draw the index label above each nail (UI-only)

    // IDE-style three-pane layout. Left + right sidebars hold all the panels
    // (Parameters/Style tabs on the left, Layers/Presets/Animation tabs on
    // the right); the canvas fills the middle. Widths are draggable via the
    // splitters between sidebar and canvas. Clamped to [180, screen_w * 0.45]
    // so the canvas can't collapse to zero width.
    float left_panel_width  = 320.0f;
    float right_panel_width = 320.0f;
    caustic::Color nail_active_color    {0.9, 0.9, 0.9, 1.0};  // chord start colour
    caustic::Color nail_active_color_end{0.9, 0.9, 0.9, 1.0};  // chord end colour (== start = solid)
    // Per-chord stroke for newly-placed chords in AddChord mode. When equal to
    // the defaults below, AddChord skips pushing to chord_widths/chord_opacities
    // so the layer style's stroke wins by default. Different from defaults
    // implies the user actively wants overrides, so we populate the buffers.
    double nail_active_width   = 0.8;
    double nail_active_opacity = 0.6;
    static constexpr double nail_default_width   = 0.8;
    static constexpr double nail_default_opacity = 0.6;

    // Undo/redo for the *current layer's* CustomChordParams only. Snapshot-
    // based: every atomic edit (add nail, add chord, delete, recolour, clear)
    // pushes the pre-edit state onto nail_undo_stack and clears nail_redo_stack.
    // Stacks are cleared when the current layer or generator type changes — no
    // cross-layer history, keeps things predictable.
    std::vector<caustic::CustomChordParams> nail_undo_stack;
    std::vector<caustic::CustomChordParams> nail_redo_stack;
    int nail_history_last_layer_idx = -1;
    caustic::GeneratorType nail_history_last_generator_type = caustic::GeneratorType::ModularChord;

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
    if (dir.empty()) return out;
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

// Resolve the directory the running binary lives in, via /proc/self/exe.
// Returns an empty path on non-Linux systems or on read failure — callers
// must tolerate that.
fs::path executable_dir() {
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
    if (ec || exe.empty()) return {};
    return exe.parent_path();
}

// Find the bundled-presets directory. Tries, in order:
//   1. ./presets   — repo-root dev workflow (`./build/app/caustic` from repo root)
//   2. <exe>/../share/caustic/presets — standard install / AppImage layout
//   3. <exe>/presets                  — portable side-by-side layout
// Returns the first directory that exists, or an empty path if none does.
fs::path find_bundled_preset_dir() {
    std::error_code ec;
    const fs::path cwd_presets = "presets";
    if (fs::is_directory(cwd_presets, ec)) return cwd_presets;
    const fs::path exe_dir = executable_dir();
    if (!exe_dir.empty()) {
        const fs::path share = exe_dir / ".." / "share" / "caustic" / "presets";
        if (fs::is_directory(share, ec)) return fs::weakly_canonical(share, ec);
        const fs::path alt = exe_dir / "presets";
        if (fs::is_directory(alt, ec)) return alt;
    }
    return {};
}

void refresh_preset_lists(AppState& state) {
    state.bundled_presets = list_presets(find_bundled_preset_dir());
    state.user_presets = list_presets(caustic::user_preset_dir());
}

// ---------------------------------------------------------------------------
// Preset thumbnail cache. Lazily renders each preset into a small Texture2D on
// first display in the preset panel. Subsequent frames reuse the texture.
//
// Uses one shared small RaylibRenderer (allocated once in main) as the
// offscreen draw target; per-preset, we render → copy texture → flip vertical
// → upload as Texture2D → store. Failed renders are remembered so we don't
// retry a broken preset every frame.

struct ThumbnailCache {
    int size_px = 96;
    std::unordered_map<std::string, Texture2D> textures;
    std::unordered_set<std::string> failed;

    ~ThumbnailCache() { clear(); }

    void clear() {
        for (auto& [_, tex] : textures) UnloadTexture(tex);
        textures.clear();
        failed.clear();
    }

    void invalidate(const fs::path& path) {
        const std::string key = path.string();
        auto it = textures.find(key);
        if (it != textures.end()) {
            UnloadTexture(it->second);
            textures.erase(it);
        }
        failed.erase(key);
    }

    // Returns a pointer to the cached texture or renders one on the spot.
    // nullptr means rendering failed (cached in `failed` so we don't retry).
    const Texture2D* get_or_render(const fs::path& path,
                                   caustic::RaylibRenderer& mini) {
        const std::string key = path.string();
        if (failed.count(key)) return nullptr;
        auto it = textures.find(key);
        if (it != textures.end()) return &it->second;
        try {
            caustic::Preset preset = caustic::load_preset(path);
            // Render at neutral camera so the thumbnail always shows the
            // canonical framing — the user's saved pan/zoom is irrelevant
            // at thumbnail scale.
            caustic::CameraState cam{};
            mini.redraw(caustic::build_renderables(preset.scene, /*coarse=*/true),
                        preset.scene.background, cam);
            Image img = LoadImageFromTexture(mini.canvas_texture());
            ImageFlipVertical(&img);
            Texture2D tex = LoadTextureFromImage(img);
            UnloadImage(img);
            auto [ins_it, _] = textures.emplace(key, tex);
            return &ins_it->second;
        } catch (const std::exception&) {
            failed.insert(key);
            return nullptr;
        }
    }
};

// ---------------------------------------------------------------------------
// Preset save/open/export helpers — shared by the Preset panel and the
// Ctrl+S / Ctrl+O / Ctrl+E keyboard shortcuts.

void quick_save_preset(AppState& state, ThumbnailCache* thumbs = nullptr) {
    try {
        const std::string name = state.save_name_buf;
        const fs::path path = caustic::user_preset_dir() / (name + ".json");
        state.preset.name = name;
        caustic::save_preset(path, state.preset);
        state.status_message = "saved " + path.string();
        refresh_preset_lists(state);
        if (thumbs) thumbs->invalidate(path);
    } catch (const std::exception& e) {
        state.status_message = std::string("save failed: ") + e.what();
    }
}

void save_as_preset(AppState& state, ThumbnailCache* thumbs = nullptr) {
    const fs::path chosen = pick_save_file(
        caustic::user_preset_dir(),
        std::string(state.save_name_buf) + ".json",
        "Caustic preset (*.json)", "*.json");
    if (chosen.empty()) return;
    try {
        state.preset.name = chosen.stem().string();
        std::strncpy(state.save_name_buf, state.preset.name.c_str(),
                     sizeof(state.save_name_buf) - 1);
        state.save_name_buf[sizeof(state.save_name_buf) - 1] = '\0';
        caustic::save_preset(chosen, state.preset);
        state.status_message = "saved " + chosen.string();
        refresh_preset_lists(state);
        if (thumbs) thumbs->invalidate(chosen);
    } catch (const std::exception& e) {
        state.status_message = std::string("save failed: ") + e.what();
    }
}

void load_preset_path(AppState& state, const fs::path& path) {
    try {
        state.preset = caustic::load_preset(path);
        std::strncpy(state.save_name_buf, state.preset.name.c_str(),
                     sizeof(state.save_name_buf) - 1);
        state.save_name_buf[sizeof(state.save_name_buf) - 1] = '\0';
        state.status_message = "loaded " + path.string();
        state.dirty = true;
    } catch (const std::exception& e) {
        state.status_message = std::string("load failed: ") + e.what();
    }
}

void open_preset_dialog(AppState& state) {
    const fs::path chosen = pick_open_file(
        caustic::user_preset_dir(),
        "Caustic preset (*.json)", "*.json");
    if (!chosen.empty()) load_preset_path(state, chosen);
}

void export_svg_to_path(AppState& state, const fs::path& path) {
    try {
        caustic::SvgOptions opts;
        opts.width = static_cast<double>(state.export_size);
        opts.height = static_cast<double>(state.export_size);
        opts.plotter_mode = state.export_plotter_mode;
        caustic::write_svg(path,
                           caustic::build_renderables(state.preset.scene, /*coarse=*/false),
                           state.preset.scene.background, opts);
        state.status_message = "exported " + path.string();
    } catch (const std::exception& e) {
        state.status_message = std::string("export failed: ") + e.what();
    }
}

// Raster export — temporarily swaps the live renderer's canvas to the requested
// export_size, redraws the scene at full quality, writes the file (PNG/JPG/BMP/
// TGA/QOI via raylib's ExportImage, which deduces format from the extension),
// restores the original canvas size and marks dirty so the live canvas refreshes
// next frame.
void export_raster_to_path(AppState& state,
                           caustic::RaylibRenderer& renderer,
                           const fs::path& path) {
    try {
        const int orig_w = renderer.width();
        const int orig_h = renderer.height();
        const int sz = std::max(64, state.export_size);
        renderer.resize(sz, sz);
        renderer.redraw(caustic::build_renderables(state.preset.scene, /*coarse=*/false),
                        state.preset.scene.background, state.preset.camera);
        const bool ok = renderer.write_image(path.string().c_str());
        renderer.resize(orig_w, orig_h);
        state.dirty = true;
        state.status_message = ok ? ("exported " + path.string())
                                  : ("export failed: write_image returned false");
    } catch (const std::exception& e) {
        state.status_message = std::string("export failed: ") + e.what();
    }
}

inline const char* export_extension(AppState::ExportFormat f) {
    switch (f) {
        case AppState::ExportFormat::Svg: return "svg";
        case AppState::ExportFormat::Png: return "png";
        case AppState::ExportFormat::Jpg: return "jpg";
    }
    return "svg";
}

inline const char* export_filter_label(AppState::ExportFormat f) {
    switch (f) {
        case AppState::ExportFormat::Svg: return "SVG (*.svg)";
        case AppState::ExportFormat::Png: return "PNG (*.png)";
        case AppState::ExportFormat::Jpg: return "JPEG (*.jpg)";
    }
    return "SVG (*.svg)";
}

inline const char* export_filter_pattern(AppState::ExportFormat f) {
    switch (f) {
        case AppState::ExportFormat::Svg: return "*.svg";
        case AppState::ExportFormat::Png: return "*.png";
        case AppState::ExportFormat::Jpg: return "*.jpg";
    }
    return "*.svg";
}

void export_image_as(AppState& state, caustic::RaylibRenderer& renderer) {
    const std::string ext = export_extension(state.export_format);
    const fs::path chosen = pick_save_file(
        user_export_dir(),
        std::string(state.export_name_buf) + "." + ext,
        export_filter_label(state.export_format),
        export_filter_pattern(state.export_format));
    if (chosen.empty()) return;
    // Ensure the chosen path has the right extension — some file pickers
    // hand back exactly what the user typed and don't auto-append.
    fs::path final_path = chosen;
    if (final_path.extension() != fs::path("." + ext)) {
        final_path += ("." + ext);
    }
    if (state.export_format == AppState::ExportFormat::Svg) {
        export_svg_to_path(state, final_path);
    } else {
        export_raster_to_path(state, renderer, final_path);
    }
}

void reset_preset_to_default(AppState& state) {
    state.preset = caustic::Preset{};
    std::strncpy(state.save_name_buf, "untitled", sizeof(state.save_name_buf) - 1);
    state.save_name_buf[sizeof(state.save_name_buf) - 1] = '\0';
    state.nail_undo_stack.clear();
    state.nail_redo_stack.clear();
    state.lenv_undo_stack.clear();
    state.lenv_redo_stack.clear();
    state.selected_nails.clear();
    state.selected_chords.clear();
    state.current_layer_idx = 0;
    state.status_message = "new preset";
    state.dirty = true;
}

// ---------------------------------------------------------------------------
// UI panels

void render_param_panel_content(AppState& state) {
    auto& p = state.current_layer();  // edit the layer the user has selected

    int gen_idx = static_cast<int>(p.generator.type);
    if (ImGui::Combo("Generator", &gen_idx, kGeneratorNames, IM_ARRAYSIZE(kGeneratorNames))) {
        p.generator.type = static_cast<caustic::GeneratorType>(gen_idx);
        state.dirty = true;
    }

    // "Surprise me" — roll a fresh parameter set from the active generator's
    // aesthetic-island stable region (see core/include/caustic/randomize.hpp
    // for the per-generator anchor lists). Greyed out for CustomChord since
    // its layout is user-authored and we won't silently wipe it.
    {
        const bool randomizable =
            caustic::generator_is_randomizable(p.generator.type);
        ImGui::BeginDisabled(!randomizable);
        if (ImGui::Button("Surprise me")) {
            // Seeded once from random_device the first time the button fires,
            // then evolves so consecutive clicks give different results. Static
            // so the seed survives between render passes.
            static std::mt19937 rng{std::random_device{}()};
            caustic::randomize_generator(p.generator, rng);
            state.dirty = true;
        }
        ImGui::EndDisabled();
        if (!randomizable) {
            ImGui::SameLine();
            ImGui::TextDisabled("(disabled — custom layout is hand-authored)");
        }
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
            // widget. The same endpoints are also draggable directly on the
            // canvas — see overlay below.
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

            ImGui::Separator();
            // Undo/redo for the canvas-drag editor.
            ImGui::BeginDisabled(state.lenv_undo_stack.empty());
            if (ImGui::Button("Undo")) {
                auto& lp = p.generator.lenv;
                state.lenv_redo_stack.push_back(lp);
                lp = std::move(state.lenv_undo_stack.back());
                state.lenv_undo_stack.pop_back();
                state.lenv_dragging_idx = -1;
                state.dirty = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(state.lenv_redo_stack.empty());
            if (ImGui::Button("Redo")) {
                auto& lp = p.generator.lenv;
                state.lenv_undo_stack.push_back(lp);
                lp = std::move(state.lenv_redo_stack.back());
                state.lenv_redo_stack.pop_back();
                state.lenv_dragging_idx = -1;
                state.dirty = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("Ctrl+Z / Ctrl+Y");
            ImGui::Checkbox("show grid", &state.preset.editor_grid.visible);
            ImGui::SameLine();
            ImGui::Checkbox("snap to grid", &state.preset.editor_grid.snap);
            {
                static const char* const kGridModes[] = {"rectangular", "polar"};
                int gm = static_cast<int>(state.preset.editor_grid.mode);
                if (ImGui::Combo("grid mode", &gm, kGridModes, IM_ARRAYSIZE(kGridModes))) {
                    state.preset.editor_grid.mode = static_cast<caustic::EditorGridMode>(gm);
                }
            }
            if (slider_double_w("grid spacing", &state.preset.editor_grid.spacing,
                                0.01, 1.0, 0.01, "%.3f")) {}
            if (state.preset.editor_grid.mode == caustic::EditorGridMode::Polar) {
                slider_int_w("polar spokes", &state.preset.editor_grid.polar_spokes, 2, 72);
            }
            ImGui::TextDisabled("Drag the a0/a1/b0/b1 handles on the canvas to reposition endpoints.");
            ImGui::TextDisabled("Drag a handle near another to snap them together (target ringed in yellow).");
            ImGui::TextDisabled("Pan: middle-click drag, Spacebar + drag, or left-click drag away from any handle");
            ImGui::TextDisabled("(Same grid + snap state as the CustomChord editor.)");
            break;
        }
        case caustic::GeneratorType::Clifford: {
            auto& c = p.generator.clif;
            if (slider_double_w("a",  &c.a,  -3.0, 3.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("b",  &c.b,  -3.0, 3.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("c",  &c.c,  -3.0, 3.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("d",  &c.d,  -3.0, 3.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("x0", &c.x0, -2.0, 2.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("y0", &c.y0, -2.0, 2.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_int_w("iterations", &c.iterations, 500, 200000)) state.dirty = true;
            if (slider_int_w("burn in",    &c.burn_in,    0,   10000))  state.dirty = true;
            static const char* const kAttractorRenderNames[] = {"polyline", "scatter", "both"};
            int rm = static_cast<int>(c.render_mode);
            if (ImGui::Combo("render mode", &rm, kAttractorRenderNames, IM_ARRAYSIZE(kAttractorRenderNames))) {
                c.render_mode = static_cast<caustic::AttractorRenderMode>(rm);
                state.dirty = true;
            }
            break;
        }
        case caustic::GeneratorType::DeJong: {
            auto& c = p.generator.dejo;
            if (slider_double_w("a",  &c.a,  -3.0, 3.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("b",  &c.b,  -3.0, 3.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("c",  &c.c,  -3.0, 3.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("d",  &c.d,  -3.0, 3.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("x0", &c.x0, -2.0, 2.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("y0", &c.y0, -2.0, 2.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_int_w("iterations", &c.iterations, 500, 200000)) state.dirty = true;
            if (slider_int_w("burn in",    &c.burn_in,    0,   10000))  state.dirty = true;
            static const char* const kAttractorRenderNames[] = {"polyline", "scatter", "both"};
            int rm = static_cast<int>(c.render_mode);
            if (ImGui::Combo("render mode", &rm, kAttractorRenderNames, IM_ARRAYSIZE(kAttractorRenderNames))) {
                c.render_mode = static_cast<caustic::AttractorRenderMode>(rm);
                state.dirty = true;
            }
            break;
        }
        case caustic::GeneratorType::Tinkerbell: {
            auto& c = p.generator.tink;
            if (slider_double_w("a",  &c.a,  -2.0, 2.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("b",  &c.b,  -2.0, 2.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("c",  &c.c,  -2.0, 2.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("d",  &c.d,  -2.0, 2.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("x0", &c.x0, -2.0, 2.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_double_w("y0", &c.y0, -2.0, 2.0, 0.001, "%.4f")) state.dirty = true;
            if (slider_int_w("iterations", &c.iterations, 500, 200000)) state.dirty = true;
            if (slider_int_w("burn in",    &c.burn_in,    0,   10000))  state.dirty = true;
            static const char* const kAttractorRenderNames[] = {"polyline", "scatter", "both"};
            int rm = static_cast<int>(c.render_mode);
            if (ImGui::Combo("render mode", &rm, kAttractorRenderNames, IM_ARRAYSIZE(kAttractorRenderNames))) {
                c.render_mode = static_cast<caustic::AttractorRenderMode>(rm);
                state.dirty = true;
            }
            ImGui::TextDisabled("Tinkerbell can diverge — orbit truncates if out of basin.");
            break;
        }
        case caustic::GeneratorType::DiamondStack: {
            auto& c = p.generator.dstack;
            if (slider_int_w("modules",  &c.n_modules,    1, 20))                state.dirty = true;
            if (slider_int_w("N",        &c.N,            2, 500))               state.dirty = true;
            if (slider_double_w("aspect", &c.aspect,      0.1, 3.0, 0.01, "%.3f")) state.dirty = true;
            if (slider_double_w("rotation", &c.rotation_rad,
                                -std::numbers::pi, std::numbers::pi, 0.01))      state.dirty = true;
            static const char* const kFanNames[] = {"both", "vertical only", "horizontal only"};
            int fan_idx = static_cast<int>(c.fans);
            if (ImGui::Combo("fans", &fan_idx, kFanNames, IM_ARRAYSIZE(kFanNames))) {
                c.fans = static_cast<caustic::DiamondStackFans>(fan_idx);
                state.dirty = true;
            }
            ImGui::TextDisabled("Two-tone hourglass: layer A fans=vertical (white), layer B fans=horizontal (red)");
            ImGui::TextDisabled("Neon star: Layers panel → Apply rotational array (N=4)");
            break;
        }
        case caustic::GeneratorType::CustomChord: {
            auto& c = p.generator.custom;

            // Snapshot the current params onto the undo stack and clear the
            // redo stack. Called *before* any mutation. Capped at 50 entries.
            auto push_undo = [&]() {
                state.nail_undo_stack.push_back(c);
                if (state.nail_undo_stack.size() > 50)
                    state.nail_undo_stack.erase(state.nail_undo_stack.begin());
                state.nail_redo_stack.clear();
            };
            auto do_undo = [&]() {
                if (state.nail_undo_stack.empty()) return;
                state.nail_redo_stack.push_back(c);
                c = std::move(state.nail_undo_stack.back());
                state.nail_undo_stack.pop_back();
                state.nail_chord_first = -1;
                state.dirty = true;
            };
            auto do_redo = [&]() {
                if (state.nail_redo_stack.empty()) return;
                state.nail_undo_stack.push_back(c);
                c = std::move(state.nail_redo_stack.back());
                state.nail_redo_stack.pop_back();
                state.nail_chord_first = -1;
                state.dirty = true;
            };

            ImGui::Text("Nails: %d   Chords: %d",
                        static_cast<int>(c.nails.size()),
                        static_cast<int>(c.chords.size()));
            ImGui::Separator();

            // Undo / Redo
            ImGui::BeginDisabled(state.nail_undo_stack.empty());
            if (ImGui::Button("Undo")) do_undo();
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::BeginDisabled(state.nail_redo_stack.empty());
            if (ImGui::Button("Redo")) do_redo();
            ImGui::EndDisabled();
            ImGui::SameLine();
            ImGui::TextDisabled("Ctrl+Z / Ctrl+Y");
            ImGui::Separator();

            ImGui::Text("Edit mode:");
            int mode_idx = static_cast<int>(state.nail_edit_mode);
            auto clear_transient = [&]() {
                state.nail_chord_first = -1;
                state.nail_dragging_idx = -1;
                state.rubber_band_active = false;
            };
            if (ImGui::RadioButton("off",            &mode_idx, 0)) { state.nail_edit_mode = NailEditMode::Off;           clear_transient(); }
            ImGui::SameLine();
            if (ImGui::RadioButton("add nail",       &mode_idx, 1)) { state.nail_edit_mode = NailEditMode::AddNail;       clear_transient(); }
            ImGui::SameLine();
            if (ImGui::RadioButton("add chord",      &mode_idx, 2)) { state.nail_edit_mode = NailEditMode::AddChord;      clear_transient(); }
            ImGui::SameLine();
            if (ImGui::RadioButton("move nail",      &mode_idx, 3)) { state.nail_edit_mode = NailEditMode::MoveNail;      clear_transient(); }
            ImGui::SameLine();
            if (ImGui::RadioButton("recolour chord", &mode_idx, 4)) { state.nail_edit_mode = NailEditMode::RecolourChord; clear_transient(); }
            ImGui::SameLine();
            if (ImGui::RadioButton("select",         &mode_idx, 5)) { state.nail_edit_mode = NailEditMode::Select;        clear_transient(); }

            if (ImGui::Button("Delete last nail") && !c.nails.empty()) {
                push_undo();
                delete_nail_in_custom(c, static_cast<int>(c.nails.size()) - 1);
                state.nail_chord_first = -1;
                state.dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete last chord") && !c.chords.empty()) {
                push_undo();
                delete_chord_in_custom(c, static_cast<int>(c.chords.size()) - 1);
                state.dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear all")) {
                push_undo();
                c.nails.clear();
                c.chords.clear();
                c.chord_colors.clear();
                c.chord_end_colors.clear();
                c.chord_widths.clear();
                c.chord_opacities.clear();
                state.nail_chord_first = -1;
                state.selected_nails.clear();
                state.selected_chords.clear();
                state.dirty = true;
            }

            // Selection summary + actions (Select mode).
            if (!state.selected_nails.empty() || !state.selected_chords.empty()) {
                ImGui::TextDisabled("Selected: %d nails, %d chords",
                                    static_cast<int>(state.selected_nails.size()),
                                    static_cast<int>(state.selected_chords.size()));
                if (ImGui::Button("Delete selected")) {
                    push_undo();
                    // Sort descending so erase-by-index doesn't shift remaining indices.
                    std::sort(state.selected_chords.begin(), state.selected_chords.end(),
                              std::greater<int>());
                    for (int idx : state.selected_chords) delete_chord_in_custom(c, idx);
                    std::sort(state.selected_nails.begin(), state.selected_nails.end(),
                              std::greater<int>());
                    for (int idx : state.selected_nails) delete_nail_in_custom(c, idx);
                    state.selected_nails.clear();
                    state.selected_chords.clear();
                    state.nail_chord_first = -1;
                    state.dirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Recolour selected") && !state.selected_chords.empty()) {
                    push_undo();
                    if (c.chord_colors.size() != c.chords.size()) {
                        c.chord_colors.assign(c.chords.size(), state.nail_active_color);
                    }
                    if (c.chord_end_colors.size() != c.chords.size()) {
                        c.chord_end_colors.assign(c.chords.size(), state.nail_active_color_end);
                    }
                    for (int idx : state.selected_chords) {
                        if (idx >= 0 && idx < static_cast<int>(c.chord_colors.size())) {
                            c.chord_colors[idx]     = state.nail_active_color;
                            c.chord_end_colors[idx] = state.nail_active_color_end;
                        }
                    }
                    state.dirty = true;
                }
                ImGui::SameLine();
                if (ImGui::Button("Clear selection")) {
                    state.selected_nails.clear();
                    state.selected_chords.clear();
                }
            }

            ImGui::Separator();
            // Active chord colours — each new chord placed by "add chord" mode
            // takes (start, end) from these pickers. Start == end → solid colour.
            // Different start/end → renderer draws a gradient along the chord.
            float ac[3] = {static_cast<float>(state.nail_active_color.r),
                           static_cast<float>(state.nail_active_color.g),
                           static_cast<float>(state.nail_active_color.b)};
            if (ImGui::ColorEdit3("start colour", ac)) {
                state.nail_active_color.r = ac[0];
                state.nail_active_color.g = ac[1];
                state.nail_active_color.b = ac[2];
                state.nail_active_color.a = 1.0;
            }
            float bc[3] = {static_cast<float>(state.nail_active_color_end.r),
                           static_cast<float>(state.nail_active_color_end.g),
                           static_cast<float>(state.nail_active_color_end.b)};
            if (ImGui::ColorEdit3("end colour",   bc)) {
                state.nail_active_color_end.r = bc[0];
                state.nail_active_color_end.g = bc[1];
                state.nail_active_color_end.b = bc[2];
                state.nail_active_color_end.a = 1.0;
            }
            if (ImGui::Button("Recolour all")) {
                push_undo();
                c.chord_colors.assign(c.chords.size(), state.nail_active_color);
                c.chord_end_colors.assign(c.chords.size(), state.nail_active_color_end);
                state.dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear chord colours")) {
                push_undo();
                c.chord_colors.clear();
                c.chord_end_colors.clear();
                state.dirty = true;
            }

            ImGui::Separator();
            ImGui::TextDisabled("Per-chord stroke (overrides layer style):");
            if (slider_double_w("active width",   &state.nail_active_width,   0.1, 8.0, 0.01, "%.2f")) {}
            if (slider_double_w("active opacity", &state.nail_active_opacity, 0.0, 1.0, 0.01, "%.2f")) {}
            if (ImGui::Button("Apply to all")) {
                push_undo();
                c.chord_widths.assign(c.chords.size(), state.nail_active_width);
                c.chord_opacities.assign(c.chords.size(), state.nail_active_opacity);
                state.dirty = true;
            }
            ImGui::SameLine();
            ImGui::BeginDisabled(state.selected_chords.empty());
            if (ImGui::Button("Apply to selected")) {
                push_undo();
                if (c.chord_widths.size() != c.chords.size()) {
                    c.chord_widths.assign(c.chords.size(), state.nail_default_width);
                }
                if (c.chord_opacities.size() != c.chords.size()) {
                    c.chord_opacities.assign(c.chords.size(), state.nail_default_opacity);
                }
                for (int idx : state.selected_chords) {
                    if (idx >= 0 && idx < static_cast<int>(c.chord_widths.size())) {
                        c.chord_widths[idx]    = state.nail_active_width;
                        c.chord_opacities[idx] = state.nail_active_opacity;
                    }
                }
                state.dirty = true;
            }
            ImGui::EndDisabled();
            ImGui::SameLine();
            if (ImGui::Button("Clear stroke overrides")) {
                push_undo();
                c.chord_widths.clear();
                c.chord_opacities.clear();
                state.dirty = true;
            }

            ImGui::Separator();
            ImGui::Checkbox("show grid", &state.preset.editor_grid.visible);
            ImGui::SameLine();
            ImGui::Checkbox("snap", &state.preset.editor_grid.snap);
            ImGui::SameLine();
            ImGui::Checkbox("pin #", &state.nail_numbers_visible);
            {
                static const char* const kGridModes[] = {"rectangular", "polar"};
                int gm = static_cast<int>(state.preset.editor_grid.mode);
                if (ImGui::Combo("grid mode", &gm, kGridModes, IM_ARRAYSIZE(kGridModes))) {
                    state.preset.editor_grid.mode = static_cast<caustic::EditorGridMode>(gm);
                }
            }
            if (slider_double_w("grid spacing", &state.preset.editor_grid.spacing,
                                0.01, 1.0, 0.01, "%.3f")) {}
            if (state.preset.editor_grid.mode == caustic::EditorGridMode::Polar) {
                slider_int_w("polar spokes", &state.preset.editor_grid.polar_spokes, 2, 72);
            }

            ImGui::TextDisabled("add nail: left-click on canvas drops a nail");
            ImGui::TextDisabled("add chord: left-click two nails to connect them — new chord takes active colours (start, end)");
            ImGui::TextDisabled("move nail: left-click + hold on a nail, drag to reposition (snap honoured)");
            ImGui::TextDisabled("recolour chord: left-click on an existing chord to apply active start/end colours");
            ImGui::TextDisabled("select: click an item to select it (Shift+click toggles); drag empty space for rubber-band; Delete removes");
            ImGui::TextDisabled("Right-click (any mode): erase the nail or chord under the cursor");
            ImGui::TextDisabled("Pan: middle-click drag, Spacebar + drag, or left-click drag on empty space");
            ImGui::TextDisabled("Save the layer as a preset to keep the pattern.");
            break;
        }
        case caustic::GeneratorType::MaurerRose: {
            auto& c = p.generator.maurer;
            if (slider_int_w("n petals",  &c.n,        1, 30))   state.dirty = true;
            if (slider_int_w("step (deg)", &c.step_deg, 1, 359))  state.dirty = true;
            if (slider_int_w("samples",   &c.samples,  60, 2000)) state.dirty = true;
            ImGui::TextDisabled("Coprime step with samples gives the dense Maurer interleave (try step=71 / samples=360).");
            break;
        }
        case caustic::GeneratorType::LissajousChord: {
            auto& c = p.generator.lichord;
            if (slider_double_w("A",   &c.A,   0.1, 5.0, 0.01)) state.dirty = true;
            if (slider_double_w("B",   &c.B,   0.1, 5.0, 0.01)) state.dirty = true;
            if (slider_int_w("a",      &c.a,   1, 50))          state.dirty = true;
            if (slider_int_w("b",      &c.b,   1, 50))          state.dirty = true;
            if (slider_double_w("phi", &c.phi, 0.0, 2.0 * std::numbers::pi, 0.01)) state.dirty = true;
            if (slider_int_w("N",      &c.N,   3, 2000))        state.dirty = true;
            if (slider_double_w("k",   &c.k,   0.0, 100.0, 0.01)) state.dirty = true;
            break;
        }
        case caustic::GeneratorType::SuperformulaChord: {
            auto& c = p.generator.supchord;
            if (slider_double_w("m",   &c.m,   0.0, 20.0, 0.1, "%.2f")) state.dirty = true;
            if (slider_double_w("n1",  &c.n1,  0.1, 100.0, 0.1, "%.2f")) state.dirty = true;
            if (slider_double_w("n2",  &c.n2,  0.0, 100.0, 0.1, "%.2f")) state.dirty = true;
            if (slider_double_w("n3",  &c.n3,  0.0, 100.0, 0.1, "%.2f")) state.dirty = true;
            if (slider_double_w("a",   &c.a,   0.1,   5.0, 0.01))        state.dirty = true;
            if (slider_double_w("b",   &c.b,   0.1,   5.0, 0.01))        state.dirty = true;
            if (slider_int_w("N",      &c.N,   3, 2000))                 state.dirty = true;
            if (slider_double_w("k",   &c.k,   0.0, 100.0, 0.01))        state.dirty = true;
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
            case caustic::GeneratorType::Clifford:          p.generator.clif     = caustic::CliffordParams{};          break;
            case caustic::GeneratorType::DeJong:            p.generator.dejo     = caustic::DeJongParams{};            break;
            case caustic::GeneratorType::Tinkerbell:        p.generator.tink     = caustic::TinkerbellParams{};        break;
            case caustic::GeneratorType::DiamondStack:      p.generator.dstack   = caustic::DiamondStackParams{};      break;
            case caustic::GeneratorType::CustomChord:       p.generator.custom   = caustic::CustomChordParams{};       state.nail_chord_first = -1; break;
            case caustic::GeneratorType::MaurerRose:        p.generator.maurer   = caustic::MaurerRoseParams{};        break;
            case caustic::GeneratorType::LissajousChord:    p.generator.lichord  = caustic::LissajousChordParams{};    break;
            case caustic::GeneratorType::SuperformulaChord: p.generator.supchord = caustic::SuperformulaChordParams{}; break;
        }
        state.dirty = true;
    }

    ImGui::Separator();
    ImGui::TextDisabled("Ctrl+click any slider to type an exact value (or right-click → Input)");
    ImGui::TextDisabled("scroll wheel on slider: ± step   Shift: ×10   Ctrl: ×0.1");
    ImGui::TextDisabled("keys 1–4: switch generator   F or 0: reset camera   F11: fullscreen");
    ImGui::TextDisabled("middle drag: pan   scroll on canvas: zoom");
}

void render_style_panel_content(AppState& state) {
    caustic::StyleSpec& s = state.current_layer().style;

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
    if (ImGui::Checkbox("cyclic", &s.cyclic)) state.dirty = true;
    ImGui::SameLine();
    ImGui::TextDisabled("(closed-curve continuity)");

    if (ImGui::Button("Reset style")) {
        // Resets the active layer's StyleSpec only. Scene-level background is
        // not touched (it's shared across all layers).
        state.current_layer().style = caustic::StyleSpec{};
        state.dirty = true;
    }

    ImGui::Separator();
    ImGui::TextDisabled("click any color square for hex/RGB/HSV input");
}

void render_layers_panel_content(AppState& state) {
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
}

// ---------------------------------------------------------------------------
// Animation panel

fs::path user_animation_dir() {
    return caustic::user_preset_dir().parent_path() / "animations";
}

// Delete per-frame intermediate files left in `dir` matching the pattern
// `<name>_NNNN.<ext>` for ext in {svg, png, jpg}. Used to tidy up after a
// single-file bake (GIF; mp4 from PNG sequence) so the user is left with
// just the final output. Returns the number of files removed. Files that
// don't match the pattern (including the single-file output itself —
// `<name>.gif`, `<name>.mp4` — and anything the user might have stashed in
// the same folder) are untouched.
int clean_intermediate_frames(const fs::path& dir, const std::string& name_prefix) {
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return 0;
    const std::string prefix = name_prefix + "_";
    int removed = 0;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (!entry.is_regular_file()) continue;
        const fs::path p = entry.path();
        const std::string ext = p.extension().string();
        if (ext != ".svg" && ext != ".png" && ext != ".jpg") continue;
        const std::string stem = p.stem().string();
        if (stem.size() < prefix.size() + 1) continue;
        if (stem.compare(0, prefix.size(), prefix) != 0) continue;
        // The chars between the prefix and the dot must all be digits — the
        // bake format is `%s_%04d.<ext>`. Bare `<name>.<ext>` (e.g. the user's
        // own preset.svg sitting in the folder) is rejected because it has
        // no underscore + digits suffix.
        const std::string num = stem.substr(prefix.size());
        const bool all_digits = !num.empty() &&
            std::all_of(num.begin(), num.end(),
                        [](unsigned char c) { return std::isdigit(c) != 0; });
        if (!all_digits) continue;
        std::error_code rm_ec;
        fs::remove(p, rm_ec);
        if (!rm_ec) ++removed;
    }
    return removed;
}

void render_animation_panel_content(AppState& state, caustic::RaylibRenderer& renderer) {
    auto& anim = state.animation;

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
        "clifford: a",
        "clifford: b",
        "clifford: c",
        "clifford: d",
        "de jong: a",
        "de jong: b",
        "de jong: c",
        "de jong: d",
        "tinkerbell: a",
        "tinkerbell: b",
        "tinkerbell: c",
        "tinkerbell: d",
        "diamond stack: aspect",
        "diamond stack: rotation",
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
    static const char* const kEnvNames[] = {"static", "linear", "sine", "keyframed"};
    if (ImGui::Combo("envelope", &env_idx, kEnvNames, IM_ARRAYSIZE(kEnvNames))) {
        switch (env_idx) {
            case 0: anim.envelope = caustic::anim::Static{};    break;
            case 1: anim.envelope = caustic::anim::Linear{};    break;
            case 2: anim.envelope = caustic::anim::Sine{};      break;
            case 3: anim.envelope = caustic::anim::Keyframed{}; break;
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
    } else if (auto* k = std::get_if<caustic::anim::Keyframed>(&anim.envelope)) {
        // One row per keyframe: t slider + value slider + delete. After any
        // edit, re-sort by t so the evaluator's bracket-find stays valid.
        ImGui::TextDisabled("%d keyframes — interpolated linearly between adjacent points", static_cast<int>(k->keys.size()));
        int delete_idx = -1;
        bool edited = false;
        for (int i = 0; i < static_cast<int>(k->keys.size()); ++i) {
            ImGui::PushID(i);
            ImGui::SetNextItemWidth(110);
            if (slider_double_w("t",     &k->keys[i].first,  0.0, 1.0, 0.01, "%.3f")) { edited = true; state.dirty = true; }
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110);
            if (slider_double_w("value", &k->keys[i].second, -100.0, 100.0, 0.01))     { edited = true; state.dirty = true; }
            ImGui::SameLine();
            if (ImGui::SmallButton("x")) { delete_idx = i; }
            ImGui::PopID();
        }
        if (delete_idx >= 0) {
            k->keys.erase(k->keys.begin() + delete_idx);
            state.dirty = true;
        }
        if (ImGui::Button("Add keyframe")) {
            // Insert at the midpoint between the last two t values, or at
            // (0.5, 0.0) if the list is empty.
            double new_t = 0.5;
            double new_v = 0.0;
            if (k->keys.size() >= 2) {
                new_t = (k->keys[k->keys.size() - 2].first + k->keys.back().first) * 0.5;
                new_v = (k->keys[k->keys.size() - 2].second + k->keys.back().second) * 0.5;
            } else if (k->keys.size() == 1) {
                new_t = std::clamp(k->keys[0].first + 0.1, 0.0, 1.0);
                new_v = k->keys[0].second;
            }
            k->keys.push_back({new_t, new_v});
            edited = true;
            state.dirty = true;
        }
        if (edited) {
            std::sort(k->keys.begin(), k->keys.end(),
                      [](const auto& a, const auto& b) { return a.first < b.first; });
        }
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
    ImGui::Text("Bake to SVG sequence (default dir: %s)", user_animation_dir().string().c_str());
    if (slider_int_w("frames", &anim.bake_frames, 2, 600)) {}
    ImGui::PushItemWidth(180);
    ImGui::InputText("name prefix", state.bake_name_buf, sizeof(state.bake_name_buf));
    ImGui::PopItemWidth();

    auto do_bake = [&](const fs::path& dir) {
        try {
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
    };

    if (ImGui::Button("Bake SVG sequence")) {
        do_bake(user_animation_dir());
    }
    ImGui::SameLine();
    if (ImGui::Button("Bake SVG to folder…")) {
        const fs::path chosen = pick_folder(user_animation_dir());
        if (!chosen.empty()) do_bake(chosen);
    }

    // PNG bake — render each frame via the raylib offscreen canvas (resized
    // to state.export_size × state.export_size), write a numbered PNG per
    // frame, optionally invoke ffmpeg to encode an mp4 from the sequence.
    auto do_bake_png = [&](const fs::path& dir) {
        try {
            fs::create_directories(dir);
            const int frames = std::max(2, anim.bake_frames);
            const int orig_w = renderer.width();
            const int orig_h = renderer.height();
            const int sz = std::max(64, state.export_size);
            renderer.resize(sz, sz);
            for (int i = 0; i < frames; ++i) {
                const double t = static_cast<double>(i) / static_cast<double>(frames - 1);
                caustic::Preset frame = state.preset;
                if (anim.target != caustic::anim::Target::None) {
                    const double value = caustic::anim::evaluate(anim.envelope, t);
                    caustic::anim::write_target(anim.target, value, frame, state.current_layer_idx);
                }
                renderer.redraw(caustic::build_renderables(frame.scene, /*coarse=*/false),
                                frame.scene.background, frame.camera);
                char buf[80];
                std::snprintf(buf, sizeof(buf), "%s_%04d.png", state.bake_name_buf, i);
                renderer.write_image((dir / buf).string().c_str());
            }
            renderer.resize(orig_w, orig_h);
            state.dirty = true;  // re-render the live canvas next frame

            std::string status = "baked " + std::to_string(frames) +
                                 " PNG frames to " + dir.string();
            if (state.encode_mp4_after_png) {
                const double fps = std::max(1.0,
                    static_cast<double>(frames) / std::max(0.001, anim.duration_sec));
                const fs::path mp4 = dir / (std::string(state.bake_name_buf) + ".mp4");
                const std::string pattern = (dir / (std::string(state.bake_name_buf) + "_%04d.png")).string();
                std::ostringstream cmd;
                cmd << "ffmpeg -y -framerate " << static_cast<int>(fps + 0.5)
                    << " -i \"" << pattern << "\""
                    << " -c:v libx264 -pix_fmt yuv420p \"" << mp4.string() << "\""
                    << " >/dev/null 2>&1";
                const int rc = std::system(cmd.str().c_str());
                if (rc == 0) {
                    status += "; encoded " + mp4.filename().string();
                    if (state.clean_bake_intermediates) {
                        const int n = clean_intermediate_frames(dir, state.bake_name_buf);
                        if (n > 0) status += "; cleaned " + std::to_string(n)
                                             + " intermediate frame" + (n == 1 ? "" : "s");
                    }
                } else {
                    status += "; ffmpeg encode failed (is `ffmpeg` on PATH?)";
                }
            }
            state.status_message = status;
        } catch (const std::exception& e) {
            state.status_message = std::string("PNG bake failed: ") + e.what();
        }
    };

    if (ImGui::Button("Bake PNG sequence")) {
        do_bake_png(user_animation_dir());
    }
    ImGui::SameLine();
    if (ImGui::Button("Bake PNG to folder…")) {
        const fs::path chosen = pick_folder(user_animation_dir());
        if (!chosen.empty()) do_bake_png(chosen);
    }
    ImGui::SameLine();
    ImGui::Checkbox("encode mp4 after", &state.encode_mp4_after_png);

    // GIF bake — single output file. Resizes the canvas, renders each frame
    // to RGBA, feeds it to msf_gif, writes the result buffer to disk.
    auto do_bake_gif = [&](const fs::path& path) {
        try {
            if (path.has_parent_path()) fs::create_directories(path.parent_path());
            const int frames = std::max(2, anim.bake_frames);
            const int orig_w = renderer.width();
            const int orig_h = renderer.height();
            const int sz = std::max(64, state.export_size);
            renderer.resize(sz, sz);

            MsfGifState gif{};
            msf_gif_begin(&gif, sz, sz);
            const int cs_per_frame = std::max(1,
                static_cast<int>(anim.duration_sec * 100.0 /
                                  static_cast<double>(frames) + 0.5));

            for (int i = 0; i < frames; ++i) {
                const double t = static_cast<double>(i) / static_cast<double>(frames - 1);
                caustic::Preset frame = state.preset;
                if (anim.target != caustic::anim::Target::None) {
                    const double value = caustic::anim::evaluate(anim.envelope, t);
                    caustic::anim::write_target(anim.target, value, frame, state.current_layer_idx);
                }
                renderer.redraw(caustic::build_renderables(frame.scene, /*coarse=*/false),
                                frame.scene.background, frame.camera);
                unsigned char* rgba = nullptr;
                int w = 0, h = 0;
                if (renderer.read_rgba_frame(&rgba, &w, &h)) {
                    msf_gif_frame(&gif, rgba, cs_per_frame, /*maxBitDepth=*/16,
                                  /*pitchInBytes=*/w * 4);
                    std::free(rgba);
                }
            }

            MsfGifResult result = msf_gif_end(&gif);
            bool wrote = false;
            if (result.data) {
                std::ofstream out(path, std::ios::binary);
                out.write(static_cast<const char*>(result.data),
                          static_cast<std::streamsize>(result.dataSize));
                wrote = out.good();
            }
            msf_gif_free(result);

            renderer.resize(orig_w, orig_h);
            state.dirty = true;

            std::string status = "baked " + std::to_string(frames) +
                                 " GIF frames to " + path.string();
            // Clean per-frame SVG / PNG / JPG intermediates left in the GIF's
            // folder from a previous Bake SVG-sequence or Bake PNG-sequence
            // run with the same name prefix. The single-file GIF makes them
            // redundant; users who want both should turn the option off.
            if (wrote && state.clean_bake_intermediates) {
                const fs::path dir = path.has_parent_path()
                                       ? path.parent_path() : fs::current_path();
                const int n = clean_intermediate_frames(dir, state.bake_name_buf);
                if (n > 0) status += "; cleaned " + std::to_string(n)
                                     + " intermediate frame" + (n == 1 ? "" : "s");
            }
            state.status_message = status;
        } catch (const std::exception& e) {
            state.status_message = std::string("GIF bake failed: ") + e.what();
        }
    };

    if (ImGui::Button("Bake animated GIF")) {
        const fs::path path = user_animation_dir() /
                              (std::string(state.bake_name_buf) + ".gif");
        do_bake_gif(path);
    }
    ImGui::SameLine();
    if (ImGui::Button("Bake GIF as…")) {
        const fs::path chosen = pick_save_file(
            user_animation_dir(),
            std::string(state.bake_name_buf) + ".gif",
            "Animated GIF (*.gif)", "*.gif");
        if (!chosen.empty()) do_bake_gif(chosen);
    }

    ImGui::Separator();
    ImGui::Checkbox("clean up frame files after GIF / mp4 bake",
                    &state.clean_bake_intermediates);
    ImGui::TextDisabled("Removes <name>_NNNN.svg / .png / .jpg in the output folder "
                        "once the single-file bake succeeds.");

    if (!state.status_message.empty()) {
        ImGui::TextDisabled("%s", state.status_message.c_str());
    }
}

void render_preset_panel_content(AppState& state,
                                 caustic::RaylibRenderer& renderer,
                                 caustic::RaylibRenderer& thumb_renderer,
                                 ThumbnailCache& thumbs) {
    // Save section. Quick "Save" writes to the default XDG dir using the
    // name field; "Save as…" opens a native file picker for arbitrary paths.
    ImGui::Text("Save (default dir: %s)", caustic::user_preset_dir().string().c_str());
    ImGui::PushItemWidth(180);
    ImGui::InputText("name", state.save_name_buf, sizeof(state.save_name_buf));
    ImGui::PopItemWidth();
    ImGui::SameLine();
    if (ImGui::Button("Save")) quick_save_preset(state, &thumbs);
    ImGui::SameLine();
    if (ImGui::Button("Save as…")) save_as_preset(state, &thumbs);
    ImGui::SameLine();
    if (ImGui::Button("New")) reset_preset_to_default(state);

    if (!state.status_message.empty()) {
        ImGui::TextDisabled("%s", state.status_message.c_str());
    }

    ImGui::Separator();

    if (ImGui::Button("Open…")) open_preset_dialog(state);
    ImGui::SameLine();
    if (ImGui::Button("Copy URL")) {
        try {
            const std::string url = caustic::encode_preset_url(state.preset);
            SetClipboardText(url.c_str());
            state.status_message = "copied preset URL (" +
                                   std::to_string(url.size()) + " chars) to clipboard";
        } catch (const std::exception& e) {
            state.status_message = std::string("copy failed: ") + e.what();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Paste URL")) {
        const char* clip = GetClipboardText();
        if (!clip || !*clip) {
            state.status_message = "clipboard is empty";
        } else if (!caustic::is_preset_url(clip)) {
            state.status_message = "clipboard text is not a caustic preset URL "
                                   "(expects 'caustic:p1:' prefix)";
        } else {
            try {
                state.preset = caustic::decode_preset_url(clip);
                std::strncpy(state.save_name_buf, state.preset.name.c_str(),
                             sizeof(state.save_name_buf) - 1);
                state.save_name_buf[sizeof(state.save_name_buf) - 1] = '\0';
                state.status_message = "loaded preset from URL";
                state.dirty = true;
            } catch (const std::exception& e) {
                state.status_message = std::string("paste failed: ") + e.what();
            }
        }
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(share via chat)");

    auto render_list = [&](const char* heading, const std::vector<fs::path>& paths) {
        if (ImGui::CollapsingHeader(heading, ImGuiTreeNodeFlags_DefaultOpen)) {
            if (paths.empty()) {
                ImGui::TextDisabled("  (none)");
            }
            const float thumb_px = 48.0f;
            for (const auto& path : paths) {
                ImGui::PushID(path.string().c_str());
                const Texture2D* tex = thumbs.get_or_render(path, thumb_renderer);
                if (tex) {
                    rlImGuiImageSize(tex, static_cast<int>(thumb_px), static_cast<int>(thumb_px));
                } else {
                    ImGui::Dummy({thumb_px, thumb_px});
                }
                ImGui::SameLine();
                if (ImGui::Selectable(path.stem().string().c_str(), false, 0,
                                      {0, thumb_px})) {
                    load_preset_path(state, path);
                }
                ImGui::PopID();
            }
        }
    };

    render_list("Bundled", state.bundled_presets);
    render_list("User", state.user_presets);

    if (ImGui::Button("Refresh")) { refresh_preset_lists(state); thumbs.clear(); }

    ImGui::Separator();
    ImGui::Text("Export");
    {
        static const char* const kExportFormatNames[] = {"SVG (vector)", "PNG", "JPEG"};
        int fmt = static_cast<int>(state.export_format);
        if (ImGui::Combo("format", &fmt, kExportFormatNames, IM_ARRAYSIZE(kExportFormatNames))) {
            state.export_format = static_cast<AppState::ExportFormat>(fmt);
        }
    }
    ImGui::PushItemWidth(180);
    ImGui::InputText("filename", state.export_name_buf, sizeof(state.export_name_buf));
    ImGui::PopItemWidth();
    ImGui::SliderInt("size", &state.export_size, 256, 4096);
    // Plotter mode is SVG-specific (pen-plotter convention: single colour, no
    // opacity, lexicographically-sorted chord order). Grey it out for raster.
    {
        const bool svg = (state.export_format == AppState::ExportFormat::Svg);
        ImGui::BeginDisabled(!svg);
        ImGui::Checkbox("plotter mode", &state.export_plotter_mode);
        ImGui::SameLine();
        ImGui::TextDisabled("(SVG only — single colour, no opacity, sorted)");
        ImGui::EndDisabled();
    }
    if (state.export_format == AppState::ExportFormat::Jpg) {
        ImGui::TextDisabled("JPEG has no alpha — the scene background fills it.");
    }

    if (ImGui::Button("Export…")) export_image_as(state, renderer);

    ImGui::TextDisabled("Shortcuts: Ctrl+S save  Ctrl+Shift+S save as  Ctrl+O open  Ctrl+E export…  Ctrl+N new");
}

// ---------------------------------------------------------------------------
// IDE-style three-pane layout
//
// Left sidebar: Parameters + Style as tabs.
// Right sidebar: Layers + Presets + Animation as tabs.
// Canvas: middle region, drawn by raylib + ImGui overlays for the editors.
// Both sidebars are draggable via splitter handles between sidebar and canvas;
// widths are clamped so the canvas can't collapse to zero.

// Vertical-axis splitter — drawn as a thin filled bar with hover/active state
// colours and an EW resize cursor. Drags update *width; clamps to [min, max].
// Called inside a dedicated borderless ImGui window so the splitter sits over
// the gap between sidebar and canvas without intruding on either.
inline void draw_v_splitter(const char* id, float* width, float min_w,
                            float max_w, float thickness, float full_height) {
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32( 55,  55,  65, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 140, 200, 255));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(130, 170, 230, 255));
    ImGui::Button(id, ImVec2(thickness, full_height));
    if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    }
    if (ImGui::IsItemActive()) {
        const float delta = ImGui::GetIO().MouseDelta.x;
        if (delta != 0.0f) {
            *width = std::clamp(*width + delta, min_w, max_w);
        }
    }
    ImGui::PopStyleColor(3);
    ImGui::PopStyleVar();
}

// Helper for the sidebar windows — fixed in place each frame, no title bar /
// resize / move so they behave like docked panes.
constexpr ImGuiWindowFlags kSidebarFlags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoBringToFrontOnFocus;

constexpr ImGuiWindowFlags kSplitterWindowFlags =
    kSidebarFlags | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoBackground;

// Push a text wrap boundary at the right edge of the current window so
// long ImGui::Text* lines (especially the TextDisabled hint blocks) wrap
// onto multiple lines instead of disappearing past the sidebar. Has no
// effect on button or slider labels — those need shorter strings.
inline void push_panel_wrap() { ImGui::PushTextWrapPos(0.0f); }
inline void pop_panel_wrap()  { ImGui::PopTextWrapPos(); }

void render_left_sidebar(AppState& state, float width, float full_h) {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, full_h), ImGuiCond_Always);
    ImGui::Begin("##left_sidebar", nullptr, kSidebarFlags);
    push_panel_wrap();
    if (ImGui::BeginTabBar("##left_tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Parameters")) {
            render_param_panel_content(state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Style")) {
            render_style_panel_content(state);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    pop_panel_wrap();
    ImGui::End();
}

void render_right_sidebar(AppState& state, float window_w, float width,
                          float full_h, caustic::RaylibRenderer& renderer,
                          caustic::RaylibRenderer& thumb_renderer,
                          ThumbnailCache& thumbs) {
    ImGui::SetNextWindowPos(ImVec2(window_w - width, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(width, full_h), ImGuiCond_Always);
    ImGui::Begin("##right_sidebar", nullptr, kSidebarFlags);
    push_panel_wrap();
    if (ImGui::BeginTabBar("##right_tabs", ImGuiTabBarFlags_None)) {
        if (ImGui::BeginTabItem("Layers")) {
            render_layers_panel_content(state);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Presets")) {
            render_preset_panel_content(state, renderer, thumb_renderer, thumbs);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Animation")) {
            render_animation_panel_content(state, renderer);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    pop_panel_wrap();
    ImGui::End();
}

}  // namespace

int main() {
    constexpr int kInitialWidth = 1280;
    constexpr int kInitialHeight = 800;

    // Silence raylib's INFO-level boot chatter — keep warnings and errors so
    // real problems (e.g. failed texture loads) still surface on stderr.
    SetTraceLogLevel(LOG_WARNING);

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
        // Dedicated small renderer for preset thumbnails so building them
        // doesn't clobber the main canvas. Reused across all thumb renders.
        caustic::RaylibRenderer thumb_renderer(96, 96);
        ThumbnailCache thumbs;
        AppState state;
        refresh_preset_lists(state);

        while (!WindowShouldClose()) {
        const ImGuiIO& io = ImGui::GetIO();

        // ---------------------------------------------------------------
        // Compute the canvas region for this frame. Three-pane layout:
        // left sidebar | splitter | canvas | splitter | right sidebar.
        // The canvas region is what raylib renders into and what every
        // mouse-to-world transform below references.
        constexpr float kSplitterThickness = 4.0f;
        const float window_w = static_cast<float>(GetScreenWidth());
        const float window_h = static_cast<float>(GetScreenHeight());
        const float max_sidebar = std::max(180.0f, window_w * 0.45f);
        state.left_panel_width  = std::clamp(state.left_panel_width,  180.0f, max_sidebar);
        state.right_panel_width = std::clamp(state.right_panel_width, 180.0f, max_sidebar);
        const int canvas_x = static_cast<int>(state.left_panel_width + kSplitterThickness);
        const int canvas_y = 0;
        const int canvas_w = std::max(1,
            static_cast<int>(window_w - state.left_panel_width - state.right_panel_width
                              - 2.0f * kSplitterThickness));
        const int canvas_h = static_cast<int>(window_h);
        // Window-space centre of the canvas — used by every overlay's
        // mouse↔world conversion.
        const double canvas_cx = canvas_x + canvas_w / 2.0;
        const double canvas_cy = canvas_y + canvas_h / 2.0;

        // Resize the offscreen render target to match the canvas region.
        // RaylibRenderer::resize already returns early when the size is
        // unchanged, so calling it every frame is cheap. The dirty flag
        // gets set whenever the size actually changes (window resize, or
        // the user dragging a sidebar splitter) so the canvas redraws.
        {
            static int last_canvas_w = -1, last_canvas_h = -1;
            if (canvas_w != last_canvas_w || canvas_h != last_canvas_h) {
                renderer.resize(canvas_w, canvas_h);
                state.dirty = true;
                last_canvas_w = canvas_w;
                last_canvas_h = canvas_h;
            }
        }

        if (!io.WantCaptureMouse) {
            // Pan rules:
            //   • Middle-click drag → always pans (every generator/mode).
            //   • Spacebar + left-drag → always pans (universal escape hatch
            //     that works even inside CustomChord edit modes).
            //   • Plain left-drag → pans when nothing editable was grabbed:
            //       - Off layers (most generators) always
            //       - LinearEnvelope if the press missed all 4 handles
            //       - CustomChord AddChord/MoveNail/RecolourChord if the
            //         press missed its target
            //       (`state.left_drag_pan` is set true on press in those
            //       miss-cases by the editor's hit-tests below.)
            const bool space_held = IsKeyDown(KEY_SPACE);
            const bool nail_editing =
                state.current_layer().generator.type == caustic::GeneratorType::CustomChord &&
                state.nail_edit_mode != NailEditMode::Off;
            const bool lenv_layer =
                state.current_layer().generator.type == caustic::GeneratorType::LinearEnvelope;
            const bool plain_left_pannable = !nail_editing && !lenv_layer;
            const bool left_down = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
            const bool pan_left = left_down && (space_held ||
                                                plain_left_pannable ||
                                                state.left_drag_pan);
            if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE) || pan_left) {
                const Vector2 d = GetMouseDelta();
                if (d.x != 0.0f || d.y != 0.0f) {
                    state.preset.camera.pan_x_px += d.x;
                    state.preset.camera.pan_y_px += d.y;
                    state.dirty = true;
                }
            }
            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                state.left_drag_pan = false;
            }
            const float wheel = GetMouseWheelMove();
            if (wheel != 0.0f) {
                const Vector2 m = GetMousePosition();
                const double cx = canvas_cx;
                const double cy = canvas_cy;
                const double factor = (wheel > 0.0f) ? 1.1 : (1.0 / 1.1);
                const double new_zoom = std::clamp(state.preset.camera.zoom * factor, 0.1, 100.0);
                const double effective = new_zoom / state.preset.camera.zoom;
                state.preset.camera.pan_x_px = (m.x - cx) * (1.0 - effective) + state.preset.camera.pan_x_px * effective;
                state.preset.camera.pan_y_px = (m.y - cy) * (1.0 - effective) + state.preset.camera.pan_y_px * effective;
                state.preset.camera.zoom = new_zoom;
                state.dirty = true;
            }

            // Custom-chord nail editor — active whenever the current layer is
            // CustomChord. Left-click does mode-specific work (AddNail,
            // AddChord, MoveNail, RecolourChord). Right-click is mode-
            // independent: it deletes the nail (or chord) under the cursor.
            auto& edit_layer = state.current_layer();
            if (edit_layer.generator.type == caustic::GeneratorType::CustomChord) {
                const Vector2 m = GetMousePosition();
                const double cx = canvas_cx;
                const double cy = canvas_cy;
                const double scale = renderer.last_fit_scale() * state.preset.camera.zoom;
                if (scale > 1e-9) {
                    double world_x = (m.x - cx - state.preset.camera.pan_x_px) / scale;
                    double world_y = -((m.y - cy - state.preset.camera.pan_y_px) / scale);
                    auto& custom = edit_layer.generator.custom;
                    auto push_undo_canvas = [&]() {
                        state.nail_undo_stack.push_back(custom);
                        if (state.nail_undo_stack.size() > 50)
                            state.nail_undo_stack.erase(state.nail_undo_stack.begin());
                        state.nail_redo_stack.clear();
                    };
                    auto find_nail_near = [&](double x, double y) -> int {
                        const double hit_radius_world = 12.0 / scale;
                        int best = -1;
                        double best_d2 = hit_radius_world * hit_radius_world;
                        for (int i = 0; i < static_cast<int>(custom.nails.size()); ++i) {
                            const double dx = custom.nails[i].x - x;
                            const double dy = custom.nails[i].y - y;
                            const double d2 = dx * dx + dy * dy;
                            if (d2 < best_d2) { best_d2 = d2; best = i; }
                        }
                        return best;
                    };
                    auto find_chord_near = [&](double x, double y) -> int {
                        const double hit_world = 10.0 / scale;
                        int best = -1;
                        double best_d = hit_world;
                        const int n_nails = static_cast<int>(custom.nails.size());
                        for (int i = 0; i < static_cast<int>(custom.chords.size()); ++i) {
                            const auto& cp = custom.chords[i];
                            if (cp.first < 0 || cp.first >= n_nails) continue;
                            if (cp.second < 0 || cp.second >= n_nails) continue;
                            const caustic::Vec2 a = custom.nails[cp.first];
                            const caustic::Vec2 b = custom.nails[cp.second];
                            const double dx = b.x - a.x;
                            const double dy = b.y - a.y;
                            const double len2 = dx*dx + dy*dy;
                            double d;
                            if (len2 < 1e-12) {
                                const double ex = x - a.x, ey = y - a.y;
                                d = std::sqrt(ex*ex + ey*ey);
                            } else {
                                double t = ((x - a.x) * dx + (y - a.y) * dy) / len2;
                                t = std::clamp(t, 0.0, 1.0);
                                const double cx_ = a.x + t * dx;
                                const double cy_ = a.y + t * dy;
                                const double ex = x - cx_, ey = y - cy_;
                                d = std::sqrt(ex*ex + ey*ey);
                            }
                            if (d < best_d) { best_d = d; best = i; }
                        }
                        return best;
                    };
                    auto maybe_snap = [&](double& x, double& y) {
                        if (state.preset.editor_grid.snap && state.preset.editor_grid.spacing > 1e-6) {
                            snap_world_point(x, y, state.preset.editor_grid.mode,
                                             state.preset.editor_grid.spacing,
                                             state.preset.editor_grid.polar_spokes);
                        }
                    };

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (state.nail_edit_mode == NailEditMode::AddNail) {
                            maybe_snap(world_x, world_y);
                            push_undo_canvas();
                            custom.nails.push_back({world_x, world_y});
                            state.dirty = true;
                        } else if (state.nail_edit_mode == NailEditMode::AddChord) {
                            const int best = find_nail_near(world_x, world_y);
                            if (best < 0) {
                                // Press missed any nail — empty-space drag pans.
                                state.left_drag_pan = true;
                            }
                            if (best >= 0) {
                                if (state.nail_chord_first < 0) {
                                    state.nail_chord_first = best;
                                } else if (state.nail_chord_first != best) {
                                    push_undo_canvas();
                                    custom.chords.push_back({state.nail_chord_first, best});
                                    custom.chord_colors.push_back(state.nail_active_color);
                                    custom.chord_end_colors.push_back(state.nail_active_color_end);
                                    if (!custom.chord_widths.empty() ||
                                        state.nail_active_width != state.nail_default_width) {
                                        if (custom.chord_widths.size() < custom.chords.size() - 1)
                                            custom.chord_widths.assign(custom.chords.size() - 1, state.nail_default_width);
                                        custom.chord_widths.push_back(state.nail_active_width);
                                    }
                                    if (!custom.chord_opacities.empty() ||
                                        state.nail_active_opacity != state.nail_default_opacity) {
                                        if (custom.chord_opacities.size() < custom.chords.size() - 1)
                                            custom.chord_opacities.assign(custom.chords.size() - 1, state.nail_default_opacity);
                                        custom.chord_opacities.push_back(state.nail_active_opacity);
                                    }
                                    state.nail_chord_first = -1;
                                    state.dirty = true;
                                } else {
                                    // Same nail clicked twice — cancel selection.
                                    state.nail_chord_first = -1;
                                }
                            }
                        } else if (state.nail_edit_mode == NailEditMode::MoveNail) {
                            const int best = find_nail_near(world_x, world_y);
                            if (best >= 0) {
                                push_undo_canvas();
                                state.nail_dragging_idx = best;
                            } else {
                                // Empty-space drag pans the canvas.
                                state.left_drag_pan = true;
                            }
                        } else if (state.nail_edit_mode == NailEditMode::RecolourChord) {
                            const int idx = find_chord_near(world_x, world_y);
                            if (idx < 0) {
                                state.left_drag_pan = true;
                            }
                            if (idx >= 0) {
                                push_undo_canvas();
                                // Pad the colour arrays so they line up with
                                // chords.size() if they were empty / partial.
                                if (custom.chord_colors.size() != custom.chords.size()) {
                                    custom.chord_colors.assign(custom.chords.size(),
                                                                state.nail_active_color);
                                }
                                if (custom.chord_end_colors.size() != custom.chords.size()) {
                                    custom.chord_end_colors.assign(custom.chords.size(),
                                                                    state.nail_active_color_end);
                                }
                                custom.chord_colors[idx]     = state.nail_active_color;
                                custom.chord_end_colors[idx] = state.nail_active_color_end;
                                state.dirty = true;
                            }
                        } else if (state.nail_edit_mode == NailEditMode::Select) {
                            const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                            const int nail_hit = find_nail_near(world_x, world_y);
                            const int chord_hit = (nail_hit < 0) ? find_chord_near(world_x, world_y) : -1;
                            auto toggle_in = [](std::vector<int>& sel, int idx) {
                                auto it = std::find(sel.begin(), sel.end(), idx);
                                if (it == sel.end()) sel.push_back(idx);
                                else sel.erase(it);
                            };
                            if (nail_hit >= 0) {
                                if (!shift) {
                                    state.selected_nails.clear();
                                    state.selected_chords.clear();
                                    state.selected_nails.push_back(nail_hit);
                                } else {
                                    toggle_in(state.selected_nails, nail_hit);
                                }
                            } else if (chord_hit >= 0) {
                                if (!shift) {
                                    state.selected_nails.clear();
                                    state.selected_chords.clear();
                                    state.selected_chords.push_back(chord_hit);
                                } else {
                                    toggle_in(state.selected_chords, chord_hit);
                                }
                            } else {
                                // Empty space — start rubber-band area select.
                                state.rubber_band_active = true;
                                state.rubber_band_start_screen = m;
                            }
                        }
                    }

                    // While the mouse button is held in MoveNail mode and we
                    // grabbed a nail on press, update its position each frame.
                    if (state.nail_edit_mode == NailEditMode::MoveNail &&
                        state.nail_dragging_idx >= 0 &&
                        state.nail_dragging_idx < static_cast<int>(custom.nails.size()) &&
                        IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                        maybe_snap(world_x, world_y);
                        caustic::Vec2& target = custom.nails[state.nail_dragging_idx];
                        if (target.x != world_x || target.y != world_y) {
                            target = {world_x, world_y};
                            state.dirty = true;
                        }
                    }

                    // Right-click: erase whatever's under the cursor. Nail
                    // hits take priority over chord hits since the hit radius
                    // is smaller and the nail is the more "primitive" target.
                    // Mode-independent — works even in mode Off. Ignored while
                    // a rubber-band drag is in progress.
                    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && !state.rubber_band_active) {
                        const int nail_idx = find_nail_near(world_x, world_y);
                        if (nail_idx >= 0) {
                            push_undo_canvas();
                            delete_nail_in_custom(custom, nail_idx);
                            state.nail_chord_first = -1;
                            state.nail_dragging_idx = -1;
                            // Drop any selection entry that referenced this index;
                            // re-index entries with > nail_idx.
                            std::vector<int> ns;
                            for (int s : state.selected_nails) {
                                if (s == nail_idx) continue;
                                ns.push_back(s > nail_idx ? s - 1 : s);
                            }
                            state.selected_nails = std::move(ns);
                            state.dirty = true;
                        } else {
                            const int chord_idx = find_chord_near(world_x, world_y);
                            if (chord_idx >= 0) {
                                push_undo_canvas();
                                delete_chord_in_custom(custom, chord_idx);
                                std::vector<int> cs;
                                for (int s : state.selected_chords) {
                                    if (s == chord_idx) continue;
                                    cs.push_back(s > chord_idx ? s - 1 : s);
                                }
                                state.selected_chords = std::move(cs);
                                state.dirty = true;
                            }
                        }
                    }

                    // Rubber-band release: when in Select mode and a rubber-
                    // band is in progress, releasing the left button finalises
                    // the area select.
                    if (state.nail_edit_mode == NailEditMode::Select &&
                        state.rubber_band_active &&
                        IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                        const double start_wx = (state.rubber_band_start_screen.x - cx - state.preset.camera.pan_x_px) / scale;
                        const double start_wy = -((state.rubber_band_start_screen.y - cy - state.preset.camera.pan_y_px) / scale);
                        const double xmin = std::min(start_wx, world_x);
                        const double xmax = std::max(start_wx, world_x);
                        const double ymin = std::min(start_wy, world_y);
                        const double ymax = std::max(start_wy, world_y);
                        const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
                        if (!shift) {
                            state.selected_nails.clear();
                            state.selected_chords.clear();
                        }
                        auto already_selected = [](const std::vector<int>& v, int idx) {
                            return std::find(v.begin(), v.end(), idx) != v.end();
                        };
                        for (int i = 0; i < static_cast<int>(custom.nails.size()); ++i) {
                            const auto& n = custom.nails[i];
                            if (n.x >= xmin && n.x <= xmax && n.y >= ymin && n.y <= ymax) {
                                if (!already_selected(state.selected_nails, i)) {
                                    state.selected_nails.push_back(i);
                                }
                            }
                        }
                        const int n_nails = static_cast<int>(custom.nails.size());
                        for (int i = 0; i < static_cast<int>(custom.chords.size()); ++i) {
                            const auto& cp = custom.chords[i];
                            if (cp.first < 0 || cp.first >= n_nails) continue;
                            if (cp.second < 0 || cp.second >= n_nails) continue;
                            const double mxw = (custom.nails[cp.first].x + custom.nails[cp.second].x) * 0.5;
                            const double myw = (custom.nails[cp.first].y + custom.nails[cp.second].y) * 0.5;
                            if (mxw >= xmin && mxw <= xmax && myw >= ymin && myw <= ymax) {
                                if (!already_selected(state.selected_chords, i)) {
                                    state.selected_chords.push_back(i);
                                }
                            }
                        }
                        state.rubber_band_active = false;
                    }
                }

                if (state.nail_edit_mode == NailEditMode::MoveNail &&
                    IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    state.nail_dragging_idx = -1;
                }
            }

            // LinearEnvelope canvas-drag editor — drag any of the 4 endpoints
            // by clicking + holding on its handle. Honours snap-to-grid (same
            // state as the CustomChord editor).
            if (edit_layer.generator.type == caustic::GeneratorType::LinearEnvelope) {
                const Vector2 m = GetMousePosition();
                const double cx = canvas_cx;
                const double cy = canvas_cy;
                const double scale = renderer.last_fit_scale() * state.preset.camera.zoom;
                if (scale > 1e-9) {
                    double world_x = (m.x - cx - state.preset.camera.pan_x_px) / scale;
                    double world_y = -((m.y - cy - state.preset.camera.pan_y_px) / scale);
                    auto& lenv = edit_layer.generator.lenv;
                    caustic::Vec2* endpoints[4] = {
                        &lenv.a_start, &lenv.a_end, &lenv.b_start, &lenv.b_end
                    };
                    auto maybe_snap = [&](double& x, double& y) {
                        if (state.preset.editor_grid.snap && state.preset.editor_grid.spacing > 1e-6) {
                            snap_world_point(x, y, state.preset.editor_grid.mode,
                                             state.preset.editor_grid.spacing,
                                             state.preset.editor_grid.polar_spokes);
                        }
                    };

                    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        const double hit_radius_world = 14.0 / scale;
                        int best = -1;
                        double best_d2 = hit_radius_world * hit_radius_world;
                        for (int i = 0; i < 4; ++i) {
                            const double dx = endpoints[i]->x - world_x;
                            const double dy = endpoints[i]->y - world_y;
                            const double d2 = dx * dx + dy * dy;
                            if (d2 < best_d2) { best_d2 = d2; best = i; }
                        }
                        if (best >= 0) {
                            state.lenv_undo_stack.push_back(lenv);
                            if (state.lenv_undo_stack.size() > 50)
                                state.lenv_undo_stack.erase(state.lenv_undo_stack.begin());
                            state.lenv_redo_stack.clear();
                            state.lenv_dragging_idx = best;
                        } else {
                            state.left_drag_pan = true;
                        }
                    }

                    if (state.lenv_dragging_idx >= 0 &&
                        state.lenv_dragging_idx < 4 &&
                        IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                        maybe_snap(world_x, world_y);
                        // Handle-to-handle snap — when the dragged endpoint
                        // comes within hit-radius of any *other* endpoint,
                        // snap to that endpoint's exact position. Lets the
                        // user rejoin a split corner (e.g. corner_fan's
                        // a_start / b_start) without pixel-hunting.
                        const double snap_r2 = (14.0 / scale) * (14.0 / scale);
                        for (int j = 0; j < 4; ++j) {
                            if (j == state.lenv_dragging_idx) continue;
                            const double dx = endpoints[j]->x - world_x;
                            const double dy = endpoints[j]->y - world_y;
                            if (dx * dx + dy * dy < snap_r2) {
                                world_x = endpoints[j]->x;
                                world_y = endpoints[j]->y;
                                break;
                            }
                        }
                        caustic::Vec2& target = *endpoints[state.lenv_dragging_idx];
                        if (target.x != world_x || target.y != world_y) {
                            target.x = world_x;
                            target.y = world_y;
                            state.dirty = true;
                        }
                    }
                }

                if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                    state.lenv_dragging_idx = -1;
                }
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

            // Delete key: remove all selected nails + chords (CustomChord).
            if (IsKeyPressed(KEY_DELETE) &&
                state.current_layer().generator.type == caustic::GeneratorType::CustomChord &&
                (!state.selected_nails.empty() || !state.selected_chords.empty())) {
                auto& cc = state.current_layer().generator.custom;
                state.nail_undo_stack.push_back(cc);
                if (state.nail_undo_stack.size() > 50)
                    state.nail_undo_stack.erase(state.nail_undo_stack.begin());
                state.nail_redo_stack.clear();
                std::sort(state.selected_chords.begin(), state.selected_chords.end(), std::greater<int>());
                for (int idx : state.selected_chords) delete_chord_in_custom(cc, idx);
                std::sort(state.selected_nails.begin(), state.selected_nails.end(), std::greater<int>());
                for (int idx : state.selected_nails) delete_nail_in_custom(cc, idx);
                state.selected_nails.clear();
                state.selected_chords.clear();
                state.nail_chord_first = -1;
                state.dirty = true;
            }

            // Ctrl+Z / Ctrl+Y (or Ctrl+Shift+Z) for CustomChord undo/redo.
            const bool ctrl = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
            const bool shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

            // Global shortcuts. Guarded with Ctrl so the bare 1..4 generator
            // hotkeys above keep working unchanged.
            if (ctrl) {
                if (IsKeyPressed(KEY_S)) {
                    if (shift) save_as_preset(state, &thumbs);
                    else       quick_save_preset(state, &thumbs);
                }
                if (IsKeyPressed(KEY_O)) open_preset_dialog(state);
                if (IsKeyPressed(KEY_E)) export_image_as(state, renderer);
                if (IsKeyPressed(KEY_N)) reset_preset_to_default(state);
            }

            if (ctrl && state.current_layer().generator.type == caustic::GeneratorType::CustomChord) {
                auto& cc = state.current_layer().generator.custom;
                if (IsKeyPressed(KEY_Z) && !shift && !state.nail_undo_stack.empty()) {
                    state.nail_redo_stack.push_back(cc);
                    cc = std::move(state.nail_undo_stack.back());
                    state.nail_undo_stack.pop_back();
                    state.nail_chord_first = -1;
                    state.dirty = true;
                } else if ((IsKeyPressed(KEY_Y) || (IsKeyPressed(KEY_Z) && shift)) &&
                           !state.nail_redo_stack.empty()) {
                    state.nail_undo_stack.push_back(cc);
                    cc = std::move(state.nail_redo_stack.back());
                    state.nail_redo_stack.pop_back();
                    state.nail_chord_first = -1;
                    state.dirty = true;
                }
            }
            if (ctrl && state.current_layer().generator.type == caustic::GeneratorType::LinearEnvelope) {
                auto& lp = state.current_layer().generator.lenv;
                if (IsKeyPressed(KEY_Z) && !shift && !state.lenv_undo_stack.empty()) {
                    state.lenv_redo_stack.push_back(lp);
                    lp = std::move(state.lenv_undo_stack.back());
                    state.lenv_undo_stack.pop_back();
                    state.lenv_dragging_idx = -1;
                    state.dirty = true;
                } else if ((IsKeyPressed(KEY_Y) || (IsKeyPressed(KEY_Z) && shift)) &&
                           !state.lenv_redo_stack.empty()) {
                    state.lenv_undo_stack.push_back(lp);
                    lp = std::move(state.lenv_redo_stack.back());
                    state.lenv_redo_stack.pop_back();
                    state.lenv_dragging_idx = -1;
                    state.dirty = true;
                }
            }
        }

        // Clear undo/redo when the active layer or its generator type changes —
        // history doesn't follow you across layers, keeps the behaviour
        // predictable.
        {
            const int cur_idx = state.current_layer_idx;
            const auto cur_type = state.current_layer().generator.type;
            if (cur_idx != state.nail_history_last_layer_idx ||
                cur_type != state.nail_history_last_generator_type) {
                state.nail_undo_stack.clear();
                state.nail_redo_stack.clear();
                state.lenv_undo_stack.clear();
                state.lenv_redo_stack.clear();
                state.lenv_dragging_idx = -1;
                state.selected_nails.clear();
                state.selected_chords.clear();
                state.rubber_band_active = false;
                state.nail_history_last_layer_idx = cur_idx;
                state.nail_history_last_generator_type = cur_type;
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
        ClearBackground(::Color{18, 18, 22, 255});  // sidebar background tint
        renderer.blit_to_screen(canvas_x, canvas_y);

        // Custom-chord nail overlay — draw the placed nails as small numbered
        // circles on top of the canvas, so the user can see where to click to
        // connect them. Only when the current layer is CustomChord.
        {
            const auto& edit_layer_for_overlay = state.current_layer();
            if (edit_layer_for_overlay.generator.type == caustic::GeneratorType::CustomChord) {
                const double cx = canvas_cx;
                const double cy = canvas_cy;
                const double scale = renderer.last_fit_scale() * state.preset.camera.zoom;
                if (scale > 1e-9) {
                    if (state.preset.editor_grid.visible) {
                        draw_grid_overlay(state.preset.editor_grid.mode,
                                          state.preset.editor_grid.spacing,
                                          state.preset.editor_grid.polar_spokes,
                                          cx, cy,
                                          state.preset.camera.pan_x_px,
                                          state.preset.camera.pan_y_px,
                                          scale,
                                          canvas_x, canvas_y, canvas_w, canvas_h);
                    }

                    const auto& custom = edit_layer_for_overlay.generator.custom;

                    // Highlight selected chords first (so they sit under the
                    // nail circles drawn next).
                    auto in_sel = [](const std::vector<int>& v, int idx) {
                        return std::find(v.begin(), v.end(), idx) != v.end();
                    };
                    const int n_nails = static_cast<int>(custom.nails.size());
                    for (int idx : state.selected_chords) {
                        if (idx < 0 || idx >= static_cast<int>(custom.chords.size())) continue;
                        const auto& cp = custom.chords[idx];
                        if (cp.first < 0 || cp.first >= n_nails) continue;
                        if (cp.second < 0 || cp.second >= n_nails) continue;
                        const float ax = static_cast<float>(cx + state.preset.camera.pan_x_px + custom.nails[cp.first].x  * scale);
                        const float ay = static_cast<float>(cy + state.preset.camera.pan_y_px - custom.nails[cp.first].y  * scale);
                        const float bx = static_cast<float>(cx + state.preset.camera.pan_x_px + custom.nails[cp.second].x * scale);
                        const float by = static_cast<float>(cy + state.preset.camera.pan_y_px - custom.nails[cp.second].y * scale);
                        DrawLineEx({ax, ay}, {bx, by}, 4.0f, ::Color{255, 230, 80, 130});
                    }

                    // AddChord mode preview line: when the user has clicked
                    // the first nail and is choosing the second, draw a live
                    // gradient line from that nail to the cursor so the next
                    // chord's appearance is visible before the second click.
                    if (state.nail_edit_mode == NailEditMode::AddChord &&
                        state.nail_chord_first >= 0 &&
                        state.nail_chord_first < n_nails) {
                        const auto& origin = custom.nails[state.nail_chord_first];
                        const float ax = static_cast<float>(cx + state.preset.camera.pan_x_px + origin.x * scale);
                        const float ay = static_cast<float>(cy + state.preset.camera.pan_y_px - origin.y * scale);
                        const Vector2 mp = GetMousePosition();
                        const auto& ca = state.nail_active_color;
                        const auto& cb = state.nail_active_color_end;
                        constexpr int kPreviewSegs = 8;
                        for (int s = 0; s < kPreviewSegs; ++s) {
                            const float t0 = static_cast<float>(s)     / kPreviewSegs;
                            const float t1 = static_cast<float>(s + 1) / kPreviewSegs;
                            const float x0 = ax + (mp.x - ax) * t0;
                            const float y0 = ay + (mp.y - ay) * t0;
                            const float x1 = ax + (mp.x - ax) * t1;
                            const float y1 = ay + (mp.y - ay) * t1;
                            const double tm = (t0 + t1) * 0.5;
                            const auto byte = [](double v) -> unsigned char {
                                return static_cast<unsigned char>(std::clamp(v * 255.0 + 0.5, 0.0, 255.0));
                            };
                            const ::Color col {
                                byte(ca.r + (cb.r - ca.r) * tm),
                                byte(ca.g + (cb.g - ca.g) * tm),
                                byte(ca.b + (cb.b - ca.b) * tm),
                                140
                            };
                            DrawLineEx({x0, y0}, {x1, y1}, 2.0f, col);
                        }
                    }

                    for (int i = 0; i < n_nails; ++i) {
                        const float sx = static_cast<float>(cx + state.preset.camera.pan_x_px + custom.nails[i].x * scale);
                        const float sy = static_cast<float>(cy + state.preset.camera.pan_y_px - custom.nails[i].y * scale);
                        const bool first_clicked = (i == state.nail_chord_first);
                        const bool is_selected   = in_sel(state.selected_nails, i);
                        const ::Color fill = first_clicked
                            ? ::Color{255, 200, 60, 255}
                            : ::Color{120, 200, 255, 220};
                        const float r = first_clicked ? 7.0f : 5.0f;
                        DrawCircle(static_cast<int>(sx), static_cast<int>(sy), r, fill);
                        DrawCircleLines(static_cast<int>(sx), static_cast<int>(sy), r, ::Color{20, 20, 20, 255});
                        if (is_selected) {
                            DrawCircleLines(static_cast<int>(sx), static_cast<int>(sy), r + 3.0f, ::Color{255, 230, 80, 255});
                            DrawCircleLines(static_cast<int>(sx), static_cast<int>(sy), r + 4.0f, ::Color{255, 230, 80, 255});
                        }
                        if (state.nail_numbers_visible) {
                            char buf[16];
                            std::snprintf(buf, sizeof(buf), "%d", i);
                            DrawText(buf, static_cast<int>(sx) + 8, static_cast<int>(sy) - 8, 12, ::Color{220, 220, 220, 255});
                        }
                    }

                    // Rubber-band rectangle (Select mode in progress).
                    if (state.rubber_band_active) {
                        const Vector2 cur = GetMousePosition();
                        const float x0 = std::min(state.rubber_band_start_screen.x, cur.x);
                        const float y0 = std::min(state.rubber_band_start_screen.y, cur.y);
                        const float w  = std::abs(cur.x - state.rubber_band_start_screen.x);
                        const float h  = std::abs(cur.y - state.rubber_band_start_screen.y);
                        DrawRectangle(static_cast<int>(x0), static_cast<int>(y0),
                                      static_cast<int>(w),  static_cast<int>(h),
                                      ::Color{200, 220, 255, 35});
                        DrawRectangleLinesEx({x0, y0, w, h}, 1.0f, ::Color{200, 220, 255, 220});
                    }
                }
            }

            // LinearEnvelope overlay: faint grid (if requested) + the two
            // segments + 4 draggable endpoint handles. Drawn after the canvas
            // blit so handles sit on top of the chord geometry.
            if (edit_layer_for_overlay.generator.type == caustic::GeneratorType::LinearEnvelope) {
                const double cx = canvas_cx;
                const double cy = canvas_cy;
                const double scale = renderer.last_fit_scale() * state.preset.camera.zoom;
                if (scale > 1e-9) {
                    if (state.preset.editor_grid.visible) {
                        draw_grid_overlay(state.preset.editor_grid.mode,
                                          state.preset.editor_grid.spacing,
                                          state.preset.editor_grid.polar_spokes,
                                          cx, cy,
                                          state.preset.camera.pan_x_px,
                                          state.preset.camera.pan_y_px,
                                          scale,
                                          canvas_x, canvas_y, canvas_w, canvas_h);
                    }

                    const auto& lenv = edit_layer_for_overlay.generator.lenv;
                    auto to_scr = [&](caustic::Vec2 v) -> Vector2 {
                        return {
                            static_cast<float>(cx + state.preset.camera.pan_x_px + v.x * scale),
                            static_cast<float>(cy + state.preset.camera.pan_y_px - v.y * scale)
                        };
                    };
                    const Vector2 a0 = to_scr(lenv.a_start);
                    const Vector2 a1 = to_scr(lenv.a_end);
                    const Vector2 b0 = to_scr(lenv.b_start);
                    const Vector2 b1 = to_scr(lenv.b_end);
                    const ::Color seg_a_col{ 90, 170, 240, 200};
                    const ::Color seg_b_col{240, 170,  90, 200};

                    DrawLineEx(a0, a1, 1.5f, seg_a_col);
                    DrawLineEx(b0, b1, 1.5f, seg_b_col);

                    const Vector2 pts[4] = {a0, a1, b0, b1};
                    const ::Color cols[4] = {seg_a_col, seg_a_col, seg_b_col, seg_b_col};
                    const char* labels[4] = {"a0", "a1", "b0", "b1"};

                    // Identify a snap target while dragging — the nearest
                    // *other* handle within hit radius — so the user can see
                    // which handle the active drag will snap onto.
                    int snap_to = -1;
                    if (state.lenv_dragging_idx >= 0 && state.lenv_dragging_idx < 4) {
                        const caustic::Vec2 src_w[4] = {lenv.a_start, lenv.a_end, lenv.b_start, lenv.b_end};
                        const caustic::Vec2 d = src_w[state.lenv_dragging_idx];
                        const double snap_r2 = (14.0 / scale) * (14.0 / scale);
                        for (int j = 0; j < 4; ++j) {
                            if (j == state.lenv_dragging_idx) continue;
                            const double dx = src_w[j].x - d.x;
                            const double dy = src_w[j].y - d.y;
                            if (dx * dx + dy * dy < snap_r2) { snap_to = j; break; }
                        }
                    }

                    for (int i = 0; i < 4; ++i) {
                        const bool dragging  = (i == state.lenv_dragging_idx);
                        const bool snap_tgt  = (i == snap_to);
                        const float r = dragging ? 7.5f : 5.5f;
                        DrawCircle(static_cast<int>(pts[i].x), static_cast<int>(pts[i].y), r, cols[i]);
                        DrawCircleLines(static_cast<int>(pts[i].x), static_cast<int>(pts[i].y), r, ::Color{20, 20, 20, 255});
                        if (snap_tgt) {
                            DrawCircleLines(static_cast<int>(pts[i].x), static_cast<int>(pts[i].y), r + 3.0f, ::Color{255, 230, 80, 255});
                            DrawCircleLines(static_cast<int>(pts[i].x), static_cast<int>(pts[i].y), r + 4.0f, ::Color{255, 230, 80, 255});
                        }
                        DrawText(labels[i], static_cast<int>(pts[i].x) + 8, static_cast<int>(pts[i].y) - 8, 11, ::Color{220, 220, 220, 255});
                    }
                }
            }
        }

        rlImGuiBegin();
        render_left_sidebar(state, state.left_panel_width, window_h);
        render_right_sidebar(state, window_w, state.right_panel_width, window_h,
                             renderer, thumb_renderer, thumbs);

        // Splitter handles between sidebars and canvas. Each gets its own
        // borderless ImGui window positioned exactly over the seam, so the
        // drag hit-area is well-defined and ImGui's WantCaptureMouse flag
        // correctly absorbs the click before raylib's pan logic sees it.
        ImGui::SetNextWindowPos(ImVec2(state.left_panel_width, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(kSplitterThickness, window_h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##left_splitter", nullptr, kSplitterWindowFlags);
        draw_v_splitter("##left_split_btn", &state.left_panel_width,
                        180.0f, max_sidebar, kSplitterThickness, window_h);
        ImGui::End();
        ImGui::PopStyleVar();

        const float right_split_x = window_w - state.right_panel_width - kSplitterThickness;
        ImGui::SetNextWindowPos(ImVec2(right_split_x, 0), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(kSplitterThickness, window_h), ImGuiCond_Always);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("##right_splitter", nullptr, kSplitterWindowFlags);
        // Inline splitter for the right pane — drag delta has the opposite
        // sign from the left pane's draw_v_splitter helper (dragging right
        // shrinks this panel, dragging left grows it).
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_Button,        IM_COL32( 55,  55,  65, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(100, 140, 200, 255));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  IM_COL32(130, 170, 230, 255));
        ImGui::Button("##right_split_btn", ImVec2(kSplitterThickness, window_h));
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }
        if (ImGui::IsItemActive()) {
            const float delta = ImGui::GetIO().MouseDelta.x;
            if (delta != 0.0f) {
                state.right_panel_width = std::clamp(
                    state.right_panel_width - delta, 180.0f, max_sidebar);
            }
        }
        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();
        ImGui::End();
        ImGui::PopStyleVar();

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
