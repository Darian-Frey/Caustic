#pragma once

#include <caustic/geometry_buffer.hpp>

#include <raylib.h>

namespace caustic {

class RaylibRenderer {
public:
    RaylibRenderer(int width, int height);
    ~RaylibRenderer();

    RaylibRenderer(const RaylibRenderer&) = delete;
    RaylibRenderer& operator=(const RaylibRenderer&) = delete;

    void redraw(const GeometryBuffer& geo);
    void blit_to_screen() const;

private:
    int width_;
    int height_;
    RenderTexture2D canvas_;
};

}  // namespace caustic
