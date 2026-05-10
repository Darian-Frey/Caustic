#include <raylib.h>
#include <imgui.h>
#include <rlImGui.h>

int main() {
    InitWindow(1280, 800, "Caustic");
    SetTargetFPS(60);
    rlImGuiSetup(true);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BLACK);

        rlImGuiBegin();
        ImGui::Begin("Caustic");
        ImGui::Text("Phase 0 — empty frame");
        ImGui::End();
        rlImGuiEnd();

        EndDrawing();
    }

    rlImGuiShutdown();
    CloseWindow();
    return 0;
}
