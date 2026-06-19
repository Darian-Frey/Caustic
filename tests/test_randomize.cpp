#include <doctest/doctest.h>

#include <random>

#include <caustic/preset.hpp>
#include <caustic/randomize.hpp>

using namespace caustic;

namespace {

// Fixed seed so the test is reproducible. The randomize functions are
// guaranteed deterministic for a given RNG state.
constexpr unsigned kSeed = 0xC0FFEEu;

// How many rolls to sample per generator. Picking 64 is enough that every
// anchor in each generator's list gets exercised at least once for the
// smaller libraries (5–12 anchors), and the range checks see a representative
// spread of the jitter intervals.
constexpr int kRolls = 64;

}  // namespace

TEST_CASE("randomize_modular_chord stays inside the documented stable range") {
    std::mt19937 rng(kSeed);
    ModularChordParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_modular_chord(p, rng);
        CHECK(p.N >= 100);
        CHECK(p.N <= 320);
        // k anchors run 2..67; the 30% jitter adds at most ±0.5.
        CHECK(p.k >= 1.5);
        CHECK(p.k <= 67.5);
    }
}

TEST_CASE("randomize_polygon_chord stays inside the documented stable range") {
    std::mt19937 rng(kSeed);
    PolygonChordParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_polygon_chord(p, rng);
        CHECK(p.n_sides >= 3);
        CHECK(p.n_sides <= 8);
        CHECK(p.N >= 100);
        CHECK(p.N <= 250);
        CHECK(p.k >= 2.0);
        CHECK(p.k <= 7.0);
        // rotation is either 0 or pi / n_sides — both bounded by pi/3 at worst.
        CHECK(p.rotation_rad >= 0.0);
        CHECK(p.rotation_rad <= 3.15 / 3.0);
    }
}

TEST_CASE("randomize_phyllotaxis lands on canonical alpha anchors") {
    std::mt19937 rng(kSeed);
    PhyllotaxisParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_phyllotaxis(p, rng);
        CHECK(p.N >= 200);
        CHECK(p.N <= 1100);
        // alpha must hit one of the five aesthetic anchors — golden, 2pi/3,
        // 2pi/5, 2pi/7, pi/2. We just check the range bound.
        CHECK(p.alpha >= 0.8);
        CHECK(p.alpha <= 2.4);
        CHECK(p.k >= 2.0);
        CHECK(p.k <= 11.0);
    }
}

TEST_CASE("randomize_linear_envelope produces sane geometry + bounded k") {
    std::mt19937 rng(kSeed);
    LinearEnvelopeParams p;
    bool saw_negative_k = false;
    bool saw_positive_k = false;
    for (int i = 0; i < kRolls; ++i) {
        randomize_linear_envelope(p, rng);
        CHECK(p.N >= 30);
        CHECK(p.N <= 80);
        CHECK((p.k == -1.0 || p.k == 1.0));
        if (p.k < 0.0) saw_negative_k = true;
        if (p.k > 0.0) saw_positive_k = true;
        // Geometry — endpoints stay within the [-1, 1] unit-square bounds.
        CHECK(std::abs(p.a_start.x) <= 1.0);
        CHECK(std::abs(p.a_start.y) <= 1.0);
        CHECK(std::abs(p.a_end.x)   <= 1.0);
        CHECK(std::abs(p.a_end.y)   <= 1.0);
    }
    // With 64 rolls at 80/20 split the negative case should always fire;
    // positive should fire too with overwhelming probability (~1 - 0.8^64 ~ 1).
    CHECK(saw_negative_k);
    CHECK(saw_positive_k);
}

TEST_CASE("randomize_hypotrochoid keeps r < R (no degenerate-curve risk)") {
    std::mt19937 rng(kSeed);
    HypotrochoidParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_hypotrochoid(p, rng);
        // BUGS.md 2026-05-10: R == r degenerates to a single point. The
        // randomiser must never produce that.
        CHECK(p.r < p.R);
        CHECK(p.R >= 5);
        CHECK(p.R <= 10);
        CHECK(p.r >= 3);
        CHECK(p.d >= 0.3 * p.R);
        CHECK(p.d <= 0.8 * p.R);
    }
}

TEST_CASE("randomize_epitrochoid produces small-integer R/r combos") {
    std::mt19937 rng(kSeed);
    EpitrochoidParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_epitrochoid(p, rng);
        CHECK(p.R >= 3);
        CHECK(p.R <= 10);
        CHECK(p.r >= 1);
        CHECK(p.r <= 5);
        CHECK(p.d >= 0.5 * p.R);
        CHECK(p.d <= 1.5 * p.R);
    }
}

TEST_CASE("randomize_lissajous keeps a/b small + phi anchored") {
    std::mt19937 rng(kSeed);
    LissajousParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_lissajous(p, rng);
        CHECK(p.a >= 1.0);
        CHECK(p.a <= 5.0);
        CHECK(p.b >= 1.0);
        CHECK(p.b <= 6.0);
        CHECK(p.phi >= 0.0);
        CHECK(p.phi <= 3.2);  // pi + epsilon
    }
}

TEST_CASE("randomize_rose stays inside the rhodonea anchor list") {
    std::mt19937 rng(kSeed);
    RoseParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_rose(p, rng);
        CHECK(p.n >= 3);
        CHECK(p.n <= 8);
        CHECK(p.d >= 1);
        CHECK(p.d <= 3);
    }
}

TEST_CASE("randomize_maurer_rose keeps n + step_deg in the necklace range") {
    std::mt19937 rng(kSeed);
    MaurerRoseParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_maurer_rose(p, rng);
        CHECK(p.n >= 2);
        CHECK(p.n <= 9);
        CHECK(p.step_deg >= 29);
        CHECK(p.step_deg <= 109);
        CHECK(p.samples == 360);
    }
}

TEST_CASE("randomize_superformula keeps m + n's inside the aesthetic islands") {
    std::mt19937 rng(kSeed);
    SuperformulaParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_superformula(p, rng);
        CHECK(p.m  >= 3.0);
        CHECK(p.m  <= 8.0);
        CHECK(p.n1 >= 0.5);
        CHECK(p.n1 <= 3.0);
        CHECK(p.n2 >= 1.0);
        CHECK(p.n2 <= 15.0);
        CHECK(p.n3 >= 1.0);
        CHECK(p.n3 <= 15.0);
    }
}

TEST_CASE("randomize_clifford jitters around catalogue anchors + scatter mode") {
    std::mt19937 rng(kSeed);
    CliffordParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_clifford(p, rng);
        // Each anchor's range expanded by the 0.05 jitter.
        CHECK(p.a >= -2.05);
        CHECK(p.a <=  1.75);
        CHECK(p.b >= -2.05);
        CHECK(p.b <=  1.75);
        CHECK(p.iterations >= 30000);
        CHECK(p.iterations <= 80000);
        CHECK(p.render_mode == AttractorRenderMode::Scatter);
    }
}

TEST_CASE("randomize_de_jong jitters around catalogue anchors + scatter mode") {
    std::mt19937 rng(kSeed);
    DeJongParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_de_jong(p, rng);
        CHECK(p.a >= -2.75);
        CHECK(p.a <=  1.75);
        CHECK(p.iterations >= 30000);
        CHECK(p.iterations <= 80000);
        CHECK(p.render_mode == AttractorRenderMode::Scatter);
    }
}

TEST_CASE("randomize_tinkerbell stays in the narrow basin") {
    std::mt19937 rng(kSeed);
    TinkerbellParams p;
    for (int i = 0; i < kRolls; ++i) {
        randomize_tinkerbell(p, rng);
        // Tinkerbell's basin is small — perturbations must stay within 0.02.
        CHECK(std::abs(p.a -  0.9)    <= 0.02);
        CHECK(std::abs(p.b - -0.6013) <= 0.02);
        CHECK(std::abs(p.c -  2.0)    <= 0.02);
        CHECK(std::abs(p.d -  0.5)    <= 0.02);
        CHECK(p.iterations >= 8000);
        CHECK(p.iterations <= 15000);
        CHECK(p.render_mode == AttractorRenderMode::Scatter);
    }
}

TEST_CASE("randomize_diamond_stack covers all three fan modes over many rolls") {
    std::mt19937 rng(kSeed);
    DiamondStackParams p;
    bool saw_both = false, saw_v = false, saw_h = false;
    for (int i = 0; i < kRolls; ++i) {
        randomize_diamond_stack(p, rng);
        CHECK(p.n_modules >= 2);
        CHECK(p.n_modules <= 6);
        CHECK(p.N >= 60);
        CHECK(p.N <= 200);
        CHECK(p.aspect >= 0.4);
        CHECK(p.aspect <= 1.2);
        if (p.fans == DiamondStackFans::Both)        saw_both = true;
        if (p.fans == DiamondStackFans::Vertical)    saw_v    = true;
        if (p.fans == DiamondStackFans::Horizontal)  saw_h    = true;
    }
    CHECK(saw_both);
    CHECK(saw_v);
    CHECK(saw_h);
}

TEST_CASE("randomize_generator dispatches to all 17 generator types") {
    // Every generator type — even CustomChord — must not crash. CustomChord is
    // a documented no-op (we don't want to wipe hand-authored layouts).
    std::mt19937 rng(kSeed);
    for (int t = 0; t <= static_cast<int>(GeneratorType::SuperformulaChord); ++t) {
        GeneratorSpec spec;
        spec.type = static_cast<GeneratorType>(t);
        randomize_generator(spec, rng);  // must not throw / segfault
        CHECK(spec.type == static_cast<GeneratorType>(t));  // type itself unchanged
    }
}

TEST_CASE("CustomChord is the only generator the randomiser leaves alone") {
    CHECK(generator_is_randomizable(GeneratorType::CustomChord) == false);
    CHECK(generator_is_randomizable(GeneratorType::ModularChord));
    CHECK(generator_is_randomizable(GeneratorType::Tinkerbell));
    CHECK(generator_is_randomizable(GeneratorType::LissajousChord));
}

TEST_CASE("randomize_generator on CustomChord leaves nails / chords untouched") {
    std::mt19937 rng(kSeed);
    GeneratorSpec spec;
    spec.type = GeneratorType::CustomChord;
    spec.custom.nails.push_back({0.5, 0.5});
    spec.custom.nails.push_back({-0.5, -0.5});
    spec.custom.chords.push_back({0, 1});
    randomize_generator(spec, rng);
    CHECK(spec.custom.nails.size()  == 2);
    CHECK(spec.custom.chords.size() == 1);
    CHECK(spec.custom.nails[0].x == 0.5);
    CHECK(spec.custom.chords[0].first == 0);
}

TEST_CASE("randomize is deterministic for a given seed") {
    std::mt19937 a(kSeed);
    std::mt19937 b(kSeed);
    ModularChordParams pa, pb;
    randomize_modular_chord(pa, a);
    randomize_modular_chord(pb, b);
    CHECK(pa.N == pb.N);
    CHECK(pa.k == pb.k);
}
