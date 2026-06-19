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

    // Blit the offscreen canvas to the window. With no args it covers the
    // whole window (legacy behaviour). With x_offset / y_offset it draws into
    // the canvas region of an IDE-style three-pane layout — the sidebars
    // overlay the rest themselves.
    void blit_to_screen(int x_offset = 0, int y_offset = 0) const;

    // Write the offscreen canvas as a PNG to the given path. Used by the
    // PNG-sequence bake. Returns true on success.
    bool write_png(const char* path) const;

    // Copy the offscreen canvas into an RGBA buffer (top-to-bottom row order,
    // 4 bytes per pixel). Buffer is allocated by the callee. Caller frees
    // via std::free. Used by the GIF encoder. Returns true on success and
    // writes the width / height through the out parameters.
    bool read_rgba_frame(unsigned char** out_pixels, int* out_w, int* out_h) const;

    // Most recent geometry fit-scale (world units → screen pixels, before
    // applying camera.zoom). Used by interactive overlays (e.g. the
    // custom-chord nail editor) to convert mouse-screen-coords back to
    // world coords. Returns 0 if no redraw has happened yet.
    double last_fit_scale() const { return last_fit_scale_; }

    // Current offscreen-canvas dimensions. Used by the PNG/GIF bake
    // functions to save the live size before temporarily resizing for export.
    int width()  const { return width_; }
    int height() const { return height_; }

    // Read-only access to the offscreen canvas's GPU texture. The texture is
    // bottom-up (a raylib RenderTexture artefact); the preset-thumbnail builder
    // flips it via ImageFlipVertical when copying out.
    const Texture2D& canvas_texture() const { return canvas_.texture; }

private:
    int width_;
    int height_;
    RenderTexture2D canvas_;
    double last_fit_scale_ = 0.0;
};

}  // namespace caustic
