# Changelog

All notable changes to Caustic will be documented in this file.
Format follows [Keep a Changelog](https://keepachangelog.com).
This project will adhere to [Semantic Versioning](https://semver.org) once a public release exists.

## [Unreleased]

### Added

- Initial documentation scaffolding: `README.md`, `CLAUDE.md`, `ROADMAP.md`, `ARCHITECTURE.md`, `SPEC.md`, `BUILD.md`, `CHANGELOG.md`.
- Architecture decisions locked: Caustic name selected; rlImGui chosen as GUI layer; v1 generator set fixed at modular chord, hypotrochoid, epitrochoid, Lissajous; on-demand rendering model defined.
- v1.1 generator roadmap expanded: superformula and phyllotaxis added to Phase 9 (parametric curve expansion); strange attractors (Clifford, de Jong, Tinkerbell) added as new Phase 10 with a dedicated iterative-orbit pipeline tier; Polish & release moved to Phase 11. L-systems and tilings considered and explicitly rejected.
- Phase 0 scaffolding landed: CMake workspace with `core/`, `render/`, `app/`, `cli/`, `tests/`; `core/include/caustic/vec2.hpp` + 3 doctest cases (all passing); `app/main.cpp` 1280×800 black window with one ImGui panel; `cli/main.cpp` prints `caustic 0.1.0`; `.gitignore` and `.editorconfig`. Dependency matrix pinned: raylib 6.0, rlImGui `Raylib_6_0`, Dear ImGui v1.92.7, nlohmann/json v3.11.3, doctest v2.4.11. `cmake --build build` succeeds; `ctest --test-dir build` is green.
- Phase 1 — modular chord MVP: `ParametricCurve` interface, `Circle`, `Chord`/`ChordSet`, `modular_chord(N, k)` generator, `RaylibRenderer` with cached `RenderTexture2D` and dirty-flag rendering loop. App displays cardioid (N=200, k=2). Idle CPU near-zero when not interacting.
- Phase 2 — roulettes + Lissajous: `HypotrochoidCurve`, `EpitrochoidCurve`, `LissajousCurve` (each a `ParametricCurve` impl); `sample_curve()` polyline sampler; `GeometryBuffer` (polylines + chords union per architecture §4.1); renderer accepts `GeometryBuffer` with fit-to-content scaling; app cycles all four generators via keys 1–4. Period computed from `gcd(R, r)` for trochoids and `gcd(a, b)` for Lissajous; non-integer ratios fall back gracefully (curve will not perfectly close, per architecture §4.2 caveat). 26 doctest cases passing.
