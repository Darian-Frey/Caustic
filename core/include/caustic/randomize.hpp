#pragma once

// Per-generator "Surprise me" — pick a parameter set from the generator's
// aesthetic-island stable region rather than uniformly across the full param
// space. Each generator has its own randomize_* function below; the
// dispatcher randomize_generator(spec, rng) switches on spec.type and calls
// the right one.
//
// Stable regions were chosen from:
//   * the bundled-preset gallery in /presets — every value that appears in a
//     hand-tuned preset is by definition aesthetic
//   * widely-known canonical values from the strange-attractor / spirograph /
//     superformula literature
//   * a small jitter range that stays inside the basin / closes the curve
//
// CustomChord is deliberately skipped — the layout is user-authored and a
// randomiser would silently wipe minutes of work. The dispatcher early-returns.
//
// The functions are deterministic given a fixed std::mt19937 seed, so tests
// can assert ranges with reproducible values.

#include <cmath>
#include <cstddef>
#include <numbers>
#include <random>

#include <caustic/preset.hpp>

namespace caustic {

// --- RNG helpers ------------------------------------------------------------

template <typename T, std::size_t N>
T random_pick(std::mt19937& rng, const T (&arr)[N]) {
    return arr[std::uniform_int_distribution<std::size_t>(0, N - 1)(rng)];
}

inline int random_int(std::mt19937& rng, int lo, int hi) {
    return std::uniform_int_distribution<int>(lo, hi)(rng);
}

inline double random_real(std::mt19937& rng, double lo, double hi) {
    return std::uniform_real_distribution<double>(lo, hi)(rng);
}

inline bool random_chance(std::mt19937& rng, double p) {
    return std::uniform_real_distribution<double>(0.0, 1.0)(rng) < p;
}

// --- Chord-set generators ---------------------------------------------------

inline void randomize_modular_chord(ModularChordParams& p, std::mt19937& rng) {
    // N anchors: counts that produce visible structure without crowding.
    constexpr int kNs[] = {120, 150, 180, 200, 240, 300};
    // k anchors: integer values that produce known closed-form envelopes —
    // cardioid (2), nephroid (3), Mathologer's times-table cusps at 51, etc.
    constexpr double kKs[] = {2.0, 3.0, 5.0, 7.0, 13.0, 17.0, 23.0, 31.0, 41.0, 51.0, 67.0};
    p.N = random_pick(rng, kNs);
    p.k = random_pick(rng, kKs);
    // 30% of rolls: small fractional offset gives the "morphing between integer
    // figures" look that's characteristic of modular chord at non-integer k.
    if (random_chance(rng, 0.3)) p.k += random_real(rng, -0.5, 0.5);
}

inline void randomize_polygon_chord(PolygonChordParams& p, std::mt19937& rng) {
    constexpr int kSides[] = {3, 4, 5, 6, 7, 8};
    constexpr int kNs[]    = {120, 150, 200, 240};
    constexpr double kKs[] = {2.0, 3.0, 5.0, 7.0};
    p.n_sides = random_pick(rng, kSides);
    p.N       = random_pick(rng, kNs);
    p.k       = random_pick(rng, kKs);
    // Rotate to align edge-down or vertex-up — both read as canonical.
    p.rotation_rad = random_chance(rng, 0.5) ? 0.0
                                              : std::numbers::pi / p.n_sides;
}

inline void randomize_phyllotaxis(PhyllotaxisParams& p, std::mt19937& rng) {
    constexpr int kNs[] = {300, 500, 700, 1000};
    // Aesthetic α anchors: the golden angle (sunflower spirals) plus other
    // rational 2π/n values. Off-anchor α is overwhelmingly chaotic so we
    // don't perturb here.
    const double kGolden = 2.39996322972865332;
    const double kAlphas[] = {kGolden, 2.0 * std::numbers::pi / 3.0,
                              2.0 * std::numbers::pi / 5.0,
                              2.0 * std::numbers::pi / 7.0,
                              std::numbers::pi / 2.0};
    constexpr double kKs[] = {2.0, 3.0, 5.0, 7.0, 11.0};
    p.N     = random_pick(rng, kNs);
    p.alpha = random_pick(rng, kAlphas);
    p.k     = random_pick(rng, kKs);
}

inline void randomize_linear_envelope(LinearEnvelopeParams& p, std::mt19937& rng) {
    // Pick from a curated geometry library. The string-art look needs k = -1
    // with shared-apex segments (the parabolic envelope rule, see MANUAL.md).
    const int kind = random_int(rng, 0, 3);
    switch (kind) {
        case 0: // Perpendicular corner fan (a up, b right) — the 1960s classic
            p.a_start = {0.0, 0.0}; p.a_end = {0.0, 1.0};
            p.b_start = {0.0, 0.0}; p.b_end = {1.0, 0.0};
            break;
        case 1: // Wider corner (60° opening)
            p.a_start = {0.0, 0.0}; p.a_end = {-0.5, 1.0};
            p.b_start = {0.0, 0.0}; p.b_end = { 1.0, 0.0};
            break;
        case 2: // Parallel-segment bowtie — sunburst through a knot point
            p.a_start = {-0.5, 0.5}; p.a_end = {-0.5, -0.5};
            p.b_start = { 0.5, 0.5}; p.b_end = { 0.5, -0.5};
            break;
        case 3: // Skewed apex — non-orthogonal corner fan
            p.a_start = {0.0, 0.0}; p.a_end = {0.3, 1.0};
            p.b_start = {0.0, 0.0}; p.b_end = {1.0, 0.2};
            break;
    }
    p.N = random_int(rng, 30, 80);
    // Parabolic envelope rule. 20% chance of k=+1 for the filled-wedge look.
    p.k = random_chance(rng, 0.8) ? -1.0 : 1.0;
}

inline void randomize_lissajous_chord(LissajousChordParams& p, std::mt19937& rng) {
    constexpr std::pair<int, int> kRatios[] = {
        {1, 2}, {1, 3}, {2, 3}, {3, 2}, {3, 4}, {4, 5}, {5, 4}, {5, 6},
    };
    const auto [a, b] = random_pick(rng, kRatios);
    p.A = 1.0;
    p.B = 1.0;
    p.a = a;
    p.b = b;
    constexpr double kPhis[] = {0.0,
                                std::numbers::pi / 4.0,
                                std::numbers::pi / 2.0,
                                3.0 * std::numbers::pi / 4.0};
    p.phi = random_pick(rng, kPhis);
    p.N   = random_int(rng, 100, 300);
    constexpr double kKs[] = {2.0, 3.0, 5.0, 7.0};
    p.k   = random_pick(rng, kKs);
}

inline void randomize_superformula_chord(SuperformulaChordParams& p, std::mt19937& rng) {
    constexpr double kMs[] = {3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    p.m  = random_pick(rng, kMs);
    p.n1 = random_real(rng, 0.5, 3.0);
    p.n2 = random_real(rng, 1.0, 15.0);
    p.n3 = random_real(rng, 1.0, 15.0);
    p.a  = 1.0;
    p.b  = 1.0;
    p.N  = random_int(rng, 100, 300);
    constexpr double kKs[] = {2.0, 3.0, 5.0, 7.0};
    p.k  = random_pick(rng, kKs);
}

inline void randomize_diamond_stack(DiamondStackParams& p, std::mt19937& rng) {
    p.n_modules    = random_int(rng, 2, 6);
    p.N            = random_int(rng, 60, 200);
    p.aspect       = random_real(rng, 0.4, 1.2);
    p.rotation_rad = random_chance(rng, 0.5) ? 0.0 : std::numbers::pi / 4.0;
    constexpr DiamondStackFans kFans[] = {
        DiamondStackFans::Both,
        DiamondStackFans::Vertical,
        DiamondStackFans::Horizontal,
    };
    p.fans = random_pick(rng, kFans);
}

// --- Parametric curves ------------------------------------------------------

inline void randomize_hypotrochoid(HypotrochoidParams& p, std::mt19937& rng) {
    // Classic Spirograph integer combos. r < R is required (r = R degenerates
    // to a point — see BUGS.md "Hypotrochoid r = R produces an empty render").
    constexpr std::pair<int, int> kRr[] = {
        {5, 3}, {6, 4}, {7, 3}, {7, 4}, {7, 5}, {8, 3}, {8, 5},
        {9, 4}, {9, 7}, {10, 3}, {10, 4}, {10, 7},
    };
    const auto [R, r] = random_pick(rng, kRr);
    p.R = R;
    p.r = r;
    // d in (0.3R, 0.8R) keeps the pen pin between centre-rolling and edge-
    // rolling, where the visual is richest.
    p.d = random_real(rng, 0.3 * R, 0.8 * R);
    p.samples = 4000;
}

inline void randomize_epitrochoid(EpitrochoidParams& p, std::mt19937& rng) {
    constexpr std::pair<int, int> kRr[] = {
        {3, 1}, {5, 2}, {5, 3}, {7, 3}, {7, 4}, {8, 3}, {8, 5},
        {10, 3}, {10, 4},
    };
    const auto [R, r] = random_pick(rng, kRr);
    p.R = R;
    p.r = r;
    p.d = random_real(rng, 0.5 * R, 1.5 * R);
    p.samples = 4000;
}

inline void randomize_lissajous(LissajousParams& p, std::mt19937& rng) {
    constexpr std::pair<int, int> kRatios[] = {
        {1, 1}, {1, 2}, {2, 3}, {3, 2}, {3, 4}, {4, 5}, {5, 4}, {5, 6},
    };
    const auto [a, b] = random_pick(rng, kRatios);
    p.A   = 1.0;
    p.B   = 1.0;
    p.a   = static_cast<double>(a);
    p.b   = static_cast<double>(b);
    constexpr double kPhis[] = {0.0,
                                std::numbers::pi / 4.0,
                                std::numbers::pi / 2.0,
                                3.0 * std::numbers::pi / 4.0,
                                std::numbers::pi};
    p.phi = random_pick(rng, kPhis);
    p.samples = 4000;
}

inline void randomize_rose(RoseParams& p, std::mt19937& rng) {
    constexpr std::pair<int, int> kNd[] = {
        {3, 1}, {4, 1}, {5, 1}, {6, 1}, {7, 1}, {8, 1},
        {3, 2}, {5, 2}, {7, 2}, {5, 3}, {7, 3}, {8, 3},
    };
    const auto [n, d] = random_pick(rng, kNd);
    p.n = n;
    p.d = d;
    p.samples = 4000;
}

inline void randomize_maurer_rose(MaurerRoseParams& p, std::mt19937& rng) {
    p.n = random_int(rng, 2, 9);
    // step_deg values that play well with samples=360 — coprime to 360 and
    // small enough that consecutive samples land on different lobes.
    constexpr int kSteps[] = {29, 31, 41, 47, 71, 89, 97, 109};
    p.step_deg = random_pick(rng, kSteps);
    p.samples  = 360;
}

inline void randomize_superformula(SuperformulaParams& p, std::mt19937& rng) {
    // m=3..8 gives 3..8-fold symmetry. n1 ~ "pointiness", n2/n3 ~ lobe shape.
    constexpr double kMs[] = {3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    p.m  = random_pick(rng, kMs);
    p.n1 = random_real(rng, 0.5, 3.0);
    p.n2 = random_real(rng, 1.0, 15.0);
    p.n3 = random_real(rng, 1.0, 15.0);
    p.a  = 1.0;
    p.b  = 1.0;
    p.samples = 4000;
}

// --- Iterative orbits -------------------------------------------------------

inline void randomize_clifford(CliffordParams& p, std::mt19937& rng) {
    // Canonical anchor sets known to produce bounded attractors. Picked from
    // Pickover's catalogue and the bundled clifford_butterfly preset.
    struct Anchor { double a, b, c, d; };
    constexpr Anchor kAnchors[] = {
        {-1.4,  1.6,  1.0,  0.7},   // canonical butterfly
        {-1.7,  1.3, -0.1, -1.21},  // pinwheel
        {-1.24, -1.25, -1.81, -1.91},
        {1.7, 1.7, 0.6, 1.2},
        {-1.8, -2.0, -0.5, -0.9},
    };
    const Anchor anchor = random_pick(rng, kAnchors);
    p.a = anchor.a + random_real(rng, -0.05, 0.05);
    p.b = anchor.b + random_real(rng, -0.05, 0.05);
    p.c = anchor.c + random_real(rng, -0.05, 0.05);
    p.d = anchor.d + random_real(rng, -0.05, 0.05);
    p.x0 = 0.1;
    p.y0 = 0.1;
    p.iterations = random_int(rng, 30000, 80000);
    p.burn_in    = 100;
    // Scatter mode matches the canonical attractor look — see Phase 11 notes.
    p.render_mode = AttractorRenderMode::Scatter;
}

inline void randomize_de_jong(DeJongParams& p, std::mt19937& rng) {
    struct Anchor { double a, b, c, d; };
    constexpr Anchor kAnchors[] = {
        { 1.4, -2.3,  2.4, -2.1},
        {-2.7, -0.09, -0.86, -2.2},
        { 1.641, 1.902, 0.316, 1.525},
        { 1.4, -2.3, -2.4,  2.1},
        {-1.0, -2.0,  1.5, -1.0},
    };
    const Anchor anchor = random_pick(rng, kAnchors);
    p.a = anchor.a + random_real(rng, -0.05, 0.05);
    p.b = anchor.b + random_real(rng, -0.05, 0.05);
    p.c = anchor.c + random_real(rng, -0.05, 0.05);
    p.d = anchor.d + random_real(rng, -0.05, 0.05);
    p.x0 = 0.1;
    p.y0 = 0.1;
    p.iterations = random_int(rng, 30000, 80000);
    p.burn_in    = 100;
    p.render_mode = AttractorRenderMode::Scatter;
}

inline void randomize_tinkerbell(TinkerbellParams& p, std::mt19937& rng) {
    // Tinkerbell's basin is narrow — perturbations beyond ~0.02 routinely
    // escape. Pin to canonical + small jitter only.
    p.a  =  0.9    + random_real(rng, -0.02, 0.02);
    p.b  = -0.6013 + random_real(rng, -0.02, 0.02);
    p.c  =  2.0    + random_real(rng, -0.02, 0.02);
    p.d  =  0.5    + random_real(rng, -0.02, 0.02);
    p.x0 = -0.72;
    p.y0 = -0.64;
    p.iterations = random_int(rng, 8000, 15000);
    p.burn_in    = 100;
    p.render_mode = AttractorRenderMode::Scatter;
}

// --- Dispatcher -------------------------------------------------------------

inline void randomize_generator(GeneratorSpec& spec, std::mt19937& rng) {
    switch (spec.type) {
        case GeneratorType::ModularChord:     randomize_modular_chord(spec.chord,     rng); break;
        case GeneratorType::Hypotrochoid:     randomize_hypotrochoid(spec.hypo,       rng); break;
        case GeneratorType::Epitrochoid:      randomize_epitrochoid(spec.epi,         rng); break;
        case GeneratorType::Lissajous:        randomize_lissajous(spec.liss,          rng); break;
        case GeneratorType::Rose:             randomize_rose(spec.rose,               rng); break;
        case GeneratorType::Superformula:     randomize_superformula(spec.supf,       rng); break;
        case GeneratorType::Phyllotaxis:      randomize_phyllotaxis(spec.phyl,        rng); break;
        case GeneratorType::PolygonChord:     randomize_polygon_chord(spec.poly,      rng); break;
        case GeneratorType::LinearEnvelope:   randomize_linear_envelope(spec.lenv,    rng); break;
        case GeneratorType::Clifford:         randomize_clifford(spec.clif,           rng); break;
        case GeneratorType::DeJong:           randomize_de_jong(spec.dejo,            rng); break;
        case GeneratorType::Tinkerbell:       randomize_tinkerbell(spec.tink,         rng); break;
        case GeneratorType::DiamondStack:     randomize_diamond_stack(spec.dstack,    rng); break;
        case GeneratorType::MaurerRose:       randomize_maurer_rose(spec.maurer,      rng); break;
        case GeneratorType::LissajousChord:   randomize_lissajous_chord(spec.lichord, rng); break;
        case GeneratorType::SuperformulaChord:randomize_superformula_chord(spec.supchord, rng); break;
        case GeneratorType::CustomChord:
            // Hand-authored layout — don't wipe the user's work on a button
            // click. The UI suppresses the "Surprise me" button for this
            // generator instead of routing here.
            break;
    }
}

// Returns true when the generator type has a randomize_* implementation.
// Used by the UI to grey out the "Surprise me" button for CustomChord.
inline bool generator_is_randomizable(GeneratorType t) {
    return t != GeneratorType::CustomChord;
}

}  // namespace caustic
