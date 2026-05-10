#pragma once

namespace caustic {

// Pan + zoom on top of the renderer's fit-to-content baseline. Pan is stored
// in screen pixels (added to the canvas-centred origin) so a drag delta in
// pixels can be applied without converting through world coordinates.
struct CameraState {
    double pan_x_px = 0.0;
    double pan_y_px = 0.0;
    double zoom = 1.0;
};

}  // namespace caustic
