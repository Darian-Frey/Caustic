#pragma once

#include <caustic/camera.hpp>
#include <caustic/geometry_buffer.hpp>
#include <caustic/style.hpp>

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

    void redraw(const GeometryBuffer& geo, const Style& style, const CameraState& camera);
    void blit_to_screen() const;

private:
    int width_;
    int height_;
    RenderTexture2D canvas_;
};

}  // namespace caustic
