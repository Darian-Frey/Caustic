#pragma once

#include <memory>

#include <caustic/colormap.hpp>
#include <caustic/preset.hpp>
#include <caustic/style.hpp>

namespace caustic {

// Build a runtime Style (with a constructed shared_ptr<ColorMap>) from a
// serializable StyleSpec.
inline Style style_from_spec(const StyleSpec& spec) {
    Style s;
    switch (spec.colormap_type) {
        case ColorMapType::Solid:
            s.color_map = std::make_shared<Solid>(spec.solid_color);
            break;
        case ColorMapType::LinearGradient:
            s.color_map = std::make_shared<LinearGradient>(spec.gradient_start, spec.gradient_end);
            break;
        case ColorMapType::HsvSweep:
            s.color_map = std::make_shared<HsvSweep>(spec.hue_start, spec.hue_end, spec.hsv_saturation, spec.hsv_value);
            break;
        case ColorMapType::Diverging:
            s.color_map = std::make_shared<Diverging>(spec.div_negative, spec.div_midpoint, spec.div_positive);
            break;
    }
    s.color_indexer = spec.color_indexer;
    s.stroke = {spec.stroke_width_min, spec.stroke_width_max, spec.stroke_width_indexer, spec.opacity};
    s.cyclic = spec.cyclic;
    return s;
}

}  // namespace caustic
