#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

#include <caustic/animation.hpp>
#include <caustic/envelope.hpp>

using namespace caustic::anim;

namespace {
constexpr double kEps = 1e-12;
}

TEST_CASE("Static envelope returns its value for any t") {
    Envelope e = Static{2.5};
    CHECK(evaluate(e, 0.0) == 2.5);
    CHECK(evaluate(e, 0.5) == 2.5);
    CHECK(evaluate(e, 1.0) == 2.5);
}

TEST_CASE("Linear envelope interpolates v0 → v1") {
    Envelope e = Linear{2.0, 3.0};
    CHECK(std::abs(evaluate(e, 0.0) - 2.0) < kEps);
    CHECK(std::abs(evaluate(e, 0.5) - 2.5) < kEps);
    CHECK(std::abs(evaluate(e, 1.0) - 3.0) < kEps);
}

TEST_CASE("Sine envelope returns offset at phase=0, t=0") {
    Envelope e = Sine{/*amplitude*/1.0, /*frequency*/1.0, /*phase*/0.0, /*offset*/0.5};
    CHECK(std::abs(evaluate(e, 0.0) - 0.5) < kEps);
    // Quarter cycle: amplitude * sin(π/2) = 1.0; result = 1.5
    CHECK(std::abs(evaluate(e, 0.25) - 1.5) < kEps);
    // Half cycle: amplitude * sin(π) ≈ 0; result ≈ 0.5
    CHECK(std::abs(evaluate(e, 0.5) - 0.5) < 1e-9);
}

TEST_CASE("write_target dispatches Modular chord k") {
    caustic::Preset p;
    p.scene.layers[0].generator.type = caustic::GeneratorType::ModularChord;
    write_target(Target::ModularChord_k, 2.5, p, 0);
    CHECK(p.scene.layers[0].generator.chord.k == 2.5);
}

TEST_CASE("write_target dispatches LayerRotate") {
    caustic::Preset p;
    write_target(Target::LayerRotate, 1.234, p, 0);
    CHECK(p.scene.layers[0].transform.rotate_rad == 1.234);
}

TEST_CASE("write_target dispatches CameraZoom (layer index ignored)") {
    caustic::Preset p;
    write_target(Target::CameraZoom, 2.0, p, 0);
    CHECK(p.camera.zoom == 2.0);
}

TEST_CASE("write_target silently no-ops when target is None") {
    caustic::Preset p;
    const double k_before = p.scene.layers[0].generator.chord.k;
    write_target(Target::None, 99.0, p, 0);
    CHECK(p.scene.layers[0].generator.chord.k == k_before);
}

TEST_CASE("write_target silently no-ops on out-of-bounds layer index") {
    caustic::Preset p;
    write_target(Target::ModularChord_k, 99.0, p, 42);
    CHECK(p.scene.layers[0].generator.chord.k != 99.0);
}

TEST_CASE("tick advances current_t when playing") {
    AnimationSpec spec;
    spec.duration_sec = 2.0;
    spec.playing = true;
    spec.current_t = 0.0;
    CHECK(tick(spec, 0.5));
    CHECK(std::abs(spec.current_t - 0.25) < kEps);  // 0.5 / 2.0
}

TEST_CASE("tick wraps current_t past 1.0") {
    AnimationSpec spec;
    spec.duration_sec = 1.0;
    spec.playing = true;
    spec.current_t = 0.9;
    tick(spec, 0.5);   // advances by 0.5, wraps from 1.4 to 0.4
    CHECK(std::abs(spec.current_t - 0.4) < kEps);
}

TEST_CASE("tick does nothing when paused") {
    AnimationSpec spec;
    spec.duration_sec = 2.0;
    spec.playing = false;
    spec.current_t = 0.3;
    CHECK_FALSE(tick(spec, 1.0));
    CHECK(spec.current_t == 0.3);
}

TEST_CASE("Acceptance: cardioid → nephroid (modular chord k, Linear 2 → 3, 60 frames)") {
    // Verifies the ROADMAP Phase 8 acceptance criterion: a Linear envelope
    // sampled at 60 frame positions gives the correct values at endpoints
    // and a smooth ramp between them.
    AnimationSpec spec;
    spec.target = Target::ModularChord_k;
    spec.envelope = Linear{2.0, 3.0};
    spec.bake_frames = 60;

    for (int i = 0; i < spec.bake_frames; ++i) {
        const double t = (spec.bake_frames > 1)
            ? static_cast<double>(i) / static_cast<double>(spec.bake_frames - 1)
            : 0.0;
        const double value = evaluate(spec.envelope, t);
        const double expected = 2.0 + (3.0 - 2.0) * t;
        CHECK(std::abs(value - expected) < kEps);
    }
}
