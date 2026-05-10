#pragma once

#include <algorithm>

#include <caustic/generators/epitrochoid.hpp>
#include <caustic/generators/hypotrochoid.hpp>
#include <caustic/generators/lissajous.hpp>
#include <caustic/generators/modular_chord.hpp>
#include <caustic/geometry_buffer.hpp>
#include <caustic/preset.hpp>
#include <caustic/sampler.hpp>

namespace caustic {

// Build a runtime GeometryBuffer from a serializable GeneratorSpec.
// `coarse=true` uses the reduced sample counts for the drag-time preview tier
// (ARCHITECTURE.md §5.4): modular chord N ÷ 4, polyline samples ÷ 2.
inline GeometryBuffer geometry_from_spec(const GeneratorSpec& g, bool coarse = false) {
    GeometryBuffer geo;
    switch (g.type) {
        case GeneratorType::ModularChord: {
            const int N = coarse ? std::max(3, g.chord.N / 4) : g.chord.N;
            geo.chords = modular_chord(N, g.chord.k);
            break;
        }
        case GeneratorType::Hypotrochoid: {
            HypotrochoidCurve curve(g.hypo.R, g.hypo.r, g.hypo.d);
            const int n = coarse ? std::max(100, g.hypo.samples / 2) : g.hypo.samples;
            geo.polylines.push_back(sample_curve(curve, n));
            break;
        }
        case GeneratorType::Epitrochoid: {
            EpitrochoidCurve curve(g.epi.R, g.epi.r, g.epi.d);
            const int n = coarse ? std::max(100, g.epi.samples / 2) : g.epi.samples;
            geo.polylines.push_back(sample_curve(curve, n));
            break;
        }
        case GeneratorType::Lissajous: {
            LissajousCurve curve(g.liss.A, g.liss.B, g.liss.a, g.liss.b, g.liss.phi);
            const int n = coarse ? std::max(100, g.liss.samples / 2) : g.liss.samples;
            geo.polylines.push_back(sample_curve(curve, n));
            break;
        }
    }
    return geo;
}

}  // namespace caustic
