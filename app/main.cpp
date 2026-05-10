#include <numbers>

#include <caustic/generators/epitrochoid.hpp>
#include <caustic/generators/hypotrochoid.hpp>
#include <caustic/generators/lissajous.hpp>
#include <caustic/generators/modular_chord.hpp>
#include <caustic/geometry_buffer.hpp>
#include <caustic/sampler.hpp>

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

caustic::GeometryBuffer build(Generator g) {
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
            renderer.redraw(build(gen));
            dirty = false;
        }

        BeginDrawing();
        ClearBackground(BLACK);
        renderer.blit_to_screen();

        rlImGuiBegin();
        ImGui::Begin("Caustic");
        ImGui::Text("Phase 2 — %s", name_of(gen));
        ImGui::Text("Press 1–4 to switch generator");
        ImGui::End();
        rlImGuiEnd();

        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
