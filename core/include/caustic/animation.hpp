#pragma once

#include <cmath>
#include <cstddef>

#include <caustic/envelope.hpp>
#include <caustic/preset.hpp>

namespace caustic::anim {

// What parameter is being animated. The enum names the (Generator, field)
// pair (or Layer/Camera-level field) directly — flat, type-safe, easy to
// dispatch via switch. Adding a new animatable parameter is: add an enum
// value here, a string entry in target_name, and a case in write_target.

enum class Target {
    None,
    // Modular chord
    ModularChord_k,
    // Hypotrochoid
    Hypotrochoid_d,
    // Epitrochoid
    Epitrochoid_d,
    // Lissajous
    Lissajous_phi,
    Lissajous_A,
    Lissajous_B,
    // Phyllotaxis
    Phyllotaxis_alpha,
    Phyllotaxis_k,
    // Polygon chord
    PolygonChord_k,
    PolygonChord_rotation,
    // Clifford attractor
    Clifford_a,
    Clifford_b,
    Clifford_c,
    Clifford_d,
    // De Jong attractor
    DeJong_a,
    DeJong_b,
    DeJong_c,
    DeJong_d,
    // Tinkerbell attractor
    Tinkerbell_a,
    Tinkerbell_b,
    Tinkerbell_c,
    Tinkerbell_d,
    // Diamond stack
    DiamondStack_aspect,
    DiamondStack_rotation,
    // Per-layer transform
    LayerRotate,
    LayerScale,
    LayerTranslateX,
    LayerTranslateY,
    // Camera
    CameraZoom,
};

inline const char* target_name(Target t) {
    switch (t) {
        case Target::None:                   return "none";
        case Target::ModularChord_k:         return "modular chord: k";
        case Target::Hypotrochoid_d:         return "hypotrochoid: d";
        case Target::Epitrochoid_d:          return "epitrochoid: d";
        case Target::Lissajous_phi:          return "lissajous: phi";
        case Target::Lissajous_A:            return "lissajous: A";
        case Target::Lissajous_B:            return "lissajous: B";
        case Target::Phyllotaxis_alpha:      return "phyllotaxis: alpha";
        case Target::Phyllotaxis_k:          return "phyllotaxis: k";
        case Target::PolygonChord_k:         return "polygon chord: k";
        case Target::PolygonChord_rotation:  return "polygon chord: rotation";
        case Target::Clifford_a:             return "clifford: a";
        case Target::Clifford_b:             return "clifford: b";
        case Target::Clifford_c:             return "clifford: c";
        case Target::Clifford_d:             return "clifford: d";
        case Target::DeJong_a:               return "de jong: a";
        case Target::DeJong_b:               return "de jong: b";
        case Target::DeJong_c:               return "de jong: c";
        case Target::DeJong_d:               return "de jong: d";
        case Target::Tinkerbell_a:           return "tinkerbell: a";
        case Target::Tinkerbell_b:           return "tinkerbell: b";
        case Target::Tinkerbell_c:           return "tinkerbell: c";
        case Target::Tinkerbell_d:           return "tinkerbell: d";
        case Target::DiamondStack_aspect:    return "diamond stack: aspect";
        case Target::DiamondStack_rotation:  return "diamond stack: rotation";
        case Target::LayerRotate:            return "layer: rotate";
        case Target::LayerScale:             return "layer: scale";
        case Target::LayerTranslateX:        return "layer: translate x";
        case Target::LayerTranslateY:        return "layer: translate y";
        case Target::CameraZoom:             return "camera: zoom";
    }
    return "?";
}

// Write `value` to the field identified by `tgt` in the given Preset. The
// `layer_idx` selects which layer's generator/transform to mutate for
// layer-scoped targets; Camera-scoped targets ignore it.
inline void write_target(Target tgt, double value, Preset& p, int layer_idx) {
    auto layer_in_bounds = [&]() {
        return layer_idx >= 0 && layer_idx < static_cast<int>(p.scene.layers.size());
    };
    switch (tgt) {
        case Target::None: break;

        case Target::ModularChord_k:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.chord.k = value;
            break;
        case Target::Hypotrochoid_d:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.hypo.d = value;
            break;
        case Target::Epitrochoid_d:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.epi.d = value;
            break;
        case Target::Lissajous_phi:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.liss.phi = value;
            break;
        case Target::Lissajous_A:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.liss.A = value;
            break;
        case Target::Lissajous_B:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.liss.B = value;
            break;
        case Target::Phyllotaxis_alpha:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.phyl.alpha = value;
            break;
        case Target::Phyllotaxis_k:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.phyl.k = value;
            break;
        case Target::PolygonChord_k:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.poly.k = value;
            break;
        case Target::PolygonChord_rotation:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.poly.rotation_rad = value;
            break;

        case Target::Clifford_a:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.clif.a = value;
            break;
        case Target::Clifford_b:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.clif.b = value;
            break;
        case Target::Clifford_c:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.clif.c = value;
            break;
        case Target::Clifford_d:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.clif.d = value;
            break;
        case Target::DeJong_a:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.dejo.a = value;
            break;
        case Target::DeJong_b:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.dejo.b = value;
            break;
        case Target::DeJong_c:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.dejo.c = value;
            break;
        case Target::DeJong_d:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.dejo.d = value;
            break;
        case Target::Tinkerbell_a:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.tink.a = value;
            break;
        case Target::Tinkerbell_b:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.tink.b = value;
            break;
        case Target::Tinkerbell_c:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.tink.c = value;
            break;
        case Target::Tinkerbell_d:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.tink.d = value;
            break;

        case Target::DiamondStack_aspect:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.dstack.aspect = value;
            break;
        case Target::DiamondStack_rotation:
            if (layer_in_bounds()) p.scene.layers[layer_idx].generator.dstack.rotation_rad = value;
            break;

        case Target::LayerRotate:
            if (layer_in_bounds()) p.scene.layers[layer_idx].transform.rotate_rad = value;
            break;
        case Target::LayerScale:
            if (layer_in_bounds()) p.scene.layers[layer_idx].transform.scale = value;
            break;
        case Target::LayerTranslateX:
            if (layer_in_bounds()) p.scene.layers[layer_idx].transform.translate.x = value;
            break;
        case Target::LayerTranslateY:
            if (layer_in_bounds()) p.scene.layers[layer_idx].transform.translate.y = value;
            break;

        case Target::CameraZoom:
            p.camera.zoom = value;
            break;
    }
}

// Animation state — what to animate, with what envelope, over what
// duration. Live in the AppState; not yet serialised into Presets (Phase 8
// is editor-only; preset-borne animation would require schema changes).
struct AnimationSpec {
    Target target = Target::None;
    Envelope envelope = Static{};
    double duration_sec = 5.0;
    int bake_frames = 60;
    bool playing = false;
    double current_t = 0.0;  // normalised [0, 1]
};

// Advance the animation's t by dt seconds and wrap back to 0 at the
// duration boundary. Returns true if the value changed.
inline bool tick(AnimationSpec& spec, double dt_sec) {
    if (!spec.playing || spec.duration_sec <= 0.0) return false;
    spec.current_t += dt_sec / spec.duration_sec;
    while (spec.current_t > 1.0) spec.current_t -= 1.0;
    while (spec.current_t < 0.0) spec.current_t += 1.0;
    return true;
}

}  // namespace caustic::anim
