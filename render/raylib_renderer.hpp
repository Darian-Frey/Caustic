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

    void redraw(const GeometryBuffer& geo, const Style& style, const CameraState& camera);
    void blit_to_screen() const;

private:
    int width_;
    int height_;
    RenderTexture2D canvas_;
};

}  // namespace caustic
