#pragma once

#include <string>
#include <vector>

#include <caustic/geometry_buffer.hpp>
#include <caustic/geometry_factory.hpp>
#include <caustic/preset.hpp>
#include <caustic/style.hpp>
#include <caustic/style_factory.hpp>

namespace caustic {

// One pre-built renderable layer: transformed geometry + runtime style + label.
// Renderers consume `std::vector<LayerRender>` plus a scene-level background.
struct LayerRender {
    GeometryBuffer geometry;
    Style style;
    std::string name;
};

// Apply a LayerTransform to every point in a GeometryBuffer in place.
inline void apply_transform(GeometryBuffer& geo, LayerTransform t) {
    for (auto& c : geo.chords) {
        c.a = apply(t, c.a);
        c.b = apply(t, c.b);
    }
    for (auto& p : geo.polylines) {
        for (auto& v : p) v = apply(t, v);
    }
}

// Build the renderable layer list from a Scene. Hidden layers are skipped.
inline std::vector<LayerRender> build_renderables(const Scene& scene, bool coarse = false) {
    std::vector<LayerRender> out;
    out.reserve(scene.layers.size());
    for (const auto& layer : scene.layers) {
        if (!layer.visible) continue;
        LayerRender r;
        r.geometry = geometry_from_spec(layer.generator, coarse);
        apply_transform(r.geometry, layer.transform);
        r.style = style_from_spec(layer.style);
        r.name = layer.name;
        out.push_back(std::move(r));
    }
    return out;
}

}  // namespace caustic
