#include <memory>
#include <numbers>

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

enum class Generator {
    ModularChord,
    Hypotrochoid,
    Epitrochoid,
    Lissajous,
};

const char* name_of(Generator g) {
    switch (g) {
        case Generator::ModularChord: return "modular chord (N=200, k=2)";
        case Generator::Hypotrochoid: return "hypotrochoid (R=5, r=3, d=2)";
        case Generator::Epitrochoid:  return "epitrochoid (R=3, r=1, d=1.5)";
        case Generator::Lissajous:    return "lissajous (a=3, b=2, phi=pi/2)";
    }
    return "?";
}

caustic::GeometryBuffer build_geometry(Generator g) {
    caustic::GeometryBuffer geo;
    switch (g) {
        case Generator::ModularChord:
            geo.chords = caustic::modular_chord(200, 2.0);
            break;
        case Generator::Hypotrochoid: {
            caustic::HypotrochoidCurve curve(5.0, 3.0, 2.0);
            geo.polylines.push_back(caustic::sample_curve(curve, 4000));
            break;
        }
        case Generator::Epitrochoid: {
            caustic::EpitrochoidCurve curve(3.0, 1.0, 1.5);
            geo.polylines.push_back(caustic::sample_curve(curve, 4000));
            break;
        }
        case Generator::Lissajous: {
            caustic::LissajousCurve curve(1.0, 1.0, 3.0, 2.0, std::numbers::pi / 2.0);
            geo.polylines.push_back(caustic::sample_curve(curve, 4000));
            break;
        }
    }
    return geo;
}

caustic::Style build_style(Generator g) {
    caustic::Style s;
    s.background = {0.04, 0.04, 0.04, 1.0};
    switch (g) {
        case Generator::ModularChord:
            // Phase 3 acceptance demo: rainbow HsvSweep indexed by chord length.
            s.color_map = std::make_shared<caustic::HsvSweep>(0.0, 360.0, 0.85, 0.95);
            s.color_indexer = caustic::Indexer::ChordLength;
            s.stroke = {0.8, 0.8, caustic::Indexer::ChordIndex, 0.6};
            break;
        case Generator::Hypotrochoid:
            // Cool→warm gradient with chord-length width modulation. Closed curve → cyclic.
            s.color_map = std::make_shared<caustic::LinearGradient>(
                caustic::Color{0.10, 0.27, 0.50}, caustic::Color{0.94, 0.75, 0.31});
            s.color_indexer = caustic::Indexer::CurveT;
            s.stroke = {1.0, 2.0, caustic::Indexer::ChordLength, 0.9};
            s.cyclic = true;
            break;
        case Generator::Epitrochoid:
            // Narrow-hue sweep keyed by segment angle. Closed curve → cyclic.
            s.color_map = std::make_shared<caustic::HsvSweep>(180.0, 260.0, 0.65, 0.9);
            s.color_indexer = caustic::Indexer::Angle;
            s.stroke = {1.2, 1.2, caustic::Indexer::ChordIndex, 0.85};
            s.cyclic = true;
            break;
        case Generator::Lissajous:
            // Diverging orange-white-cyan with width tapering along the curve. Closed → cyclic.
            s.color_map = std::make_shared<caustic::Diverging>(
                caustic::Color{0.95, 0.55, 0.20},
                caustic::Color{0.95, 0.95, 0.95},
                caustic::Color{0.20, 0.70, 0.85});
            s.color_indexer = caustic::Indexer::CurveT;
            s.stroke = {1.0, 3.0, caustic::Indexer::CurveT, 0.95};
            s.cyclic = true;
            break;
    }
    return s;
}

}  // namespace

int main() {
    constexpr int kWidth = 1280;
    constexpr int kHeight = 800;

    InitWindow(kWidth, kHeight, "Caustic");
    SetTargetFPS(60);
    rlImGuiSetup(true);

    caustic::RaylibRenderer renderer(kWidth, kHeight);
    Generator gen = Generator::ModularChord;
    bool dirty = true;

    while (!WindowShouldClose()) {
        const Generator prev = gen;
        if (IsKeyPressed(KEY_ONE))   gen = Generator::ModularChord;
        if (IsKeyPressed(KEY_TWO))   gen = Generator::Hypotrochoid;
        if (IsKeyPressed(KEY_THREE)) gen = Generator::Epitrochoid;
        if (IsKeyPressed(KEY_FOUR))  gen = Generator::Lissajous;
        if (gen != prev) dirty = true;

        if (dirty) {
            renderer.redraw(build_geometry(gen), build_style(gen));
            dirty = false;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        renderer.blit_to_screen();

        rlImGuiBegin();
        ImGui::Begin("Caustic");
        ImGui::Text("Phase 3 — %s", name_of(gen));
        ImGui::Text("Press 1–4 to switch generator");
        ImGui::End();
        rlImGuiEnd();

        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
