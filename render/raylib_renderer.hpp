#pragma once

#include <vector>

#include <caustic/camera.hpp>
#include <caustic/color.hpp>
#include <caustic/scene_render.hpp>

#include <raylib.h>

namespace caustic {

class RaylibRenderer {
public:
    RaylibRenderer(int width, int height);
    ~RaylibRenderer();

    RaylibRenderer(const RaylibRenderer&) = delete;
    RaylibRenderer& operator=(const RaylibRenderer&) = delete;

    // Reallocate the offscreen canvas to a new size. Caller should mark state
    // dirty so the next frame redraws into the fresh canvas.
    void resize(int width, int height);

    // Multi-layer redraw. Iterates layers in order, applies the scene-level
    // background as ClearBackground colour, then draws each layer's geometry
    // with its own style on top.
    void redraw(const std::vector<LayerRender>& layers,
                Color background,
                const CameraState& camera);

    void blit_to_screen() const;

private:
    int width_;
    int height_;
    RenderTexture2D canvas_;
};

}  // namespace caustic
