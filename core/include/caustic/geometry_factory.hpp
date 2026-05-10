#pragma once

#include <algorithm>

#include <caustic/generators/epitrochoid.hpp>
#include <caustic/generators/hypotrochoid.hpp>
#include <caustic/generators/linear_envelope.hpp>
#include <caustic/generators/lissajous.hpp>
#include <caustic/generators/modular_chord.hpp>
#include <caustic/generators/phyllotaxis.hpp>
#include <caustic/generators/polygon.hpp>
#include <caustic/generators/rose.hpp>
#include <caustic/generators/superformula.hpp>
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
        case GeneratorType::Rose: {
            RoseCurve curve(g.rose.n, g.rose.d);
            const int n = coarse ? std::max(100, g.rose.samples / 2) : g.rose.samples;
            geo.polylines.push_back(sample_curve(curve, n));
            break;
        }
        case GeneratorType::Superformula: {
            SuperformulaCurve curve(g.supf.m, g.supf.n1, g.supf.n2, g.supf.n3, g.supf.a, g.supf.b);
            const int n = coarse ? std::max(100, g.supf.samples / 2) : g.supf.samples;
            geo.polylines.push_back(sample_curve(curve, n));
            break;
        }
        case GeneratorType::Phyllotaxis: {
            const int N = coarse ? std::max(10, g.phyl.N / 4) : g.phyl.N;
            geo.chords = phyllotaxis_chord(N, g.phyl.alpha, g.phyl.k);
            break;
        }
        case GeneratorType::PolygonChord: {
            const int N = coarse ? std::max(3, g.poly.N / 4) : g.poly.N;
            geo.chords = polygon_chord(g.poly.n_sides, N, g.poly.k, g.poly.rotation_rad);
            break;
        }
        case GeneratorType::LinearEnvelope: {
            const int N = coarse ? std::max(3, g.lenv.N / 4) : g.lenv.N;
            geo.chords = linear_envelope(g.lenv.a_start, g.lenv.a_end,
                                          g.lenv.b_start, g.lenv.b_end,
                                          N, g.lenv.k);
            break;
        }
    }
    return geo;
}

}  // namespace caustic
