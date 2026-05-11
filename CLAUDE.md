# CLAUDE.md

## Project

Caustic is a C++20 desktop studio for generative geometric art — chord patterns ("string art") and roulette curves ("spirograph"), with live parameter editing via rlImGui and vector SVG export. Targets a freemium release on itch.io alongside an open-source GitHub repo.

## Current state

**v1 functionally complete (Phases 0–7, 2026-05-10).** Four generators (modular chord + three roulettes), full style system, rlImGui UI with live editing + scroll-wheel modifiers + coarse drag preview + camera (pan/zoom/fullscreen), preset save/load to XDG dir, SVG export with plotter mode, and a headless `caustic-cli`. `render/` is split into `caustic-render-svg` (no raylib, always built) and `caustic-render-raylib` (gated on `CAUSTIC_BUILD_APP`). Shared `caustic::geometry_from_spec` / `caustic::style_from_spec` factories let the CLI consume the same code paths as the GUI without pulling raylib in. CI on every push runs full + headless builds + batch SVG render. Build green, 64 doctest cases + 10 CLI CTest smoke cases passing.

Pinned dependency matrix (BUILD.md is the source of truth): raylib 6.0, rlImGui `Raylib_6_0`, Dear ImGui v1.92.7, nlohmann/json v3.11.3, doctest v2.4.11. rlImGui's `Raylib_*` tags name the matching raylib release — bump in lockstep.

**Phase 4 deviation worth remembering:** R/r and a/b sliders are integer-only. The architecture's "drag through irrational a/b to watch Lissajous precess" demo is unavailable until someone adds a sample-over-many-revolutions mode + toggle. d, A, B, φ stay continuous (they don't affect closure).

**Phase 5 deviation worth remembering:** `Preset.camera` stores pan as screen pixels (`pan_x_px`, `pan_y_px`) + zoom. SPEC has been updated to match the implementation. Not strictly portable across canvas sizes.

**Phase 6 deviation worth remembering:** Douglas–Peucker simplification is documented in SPEC.md §5 and plumbed through `SvgOptions::simplify_epsilon` (and the `--simplify` CLI flag) but not yet applied. Coloured polyline exports emit one `<line>` per segment — ~4000 lines for the default samples. Plotter-mode chord sorting is lexicographic-by-start-point, not true nearest-neighbour.

## Active task

**Phase 11 complete (2026-05-11). v1.1 generator set finalised — 12 generators across three pipeline tiers.** Clifford, de Jong, and Tinkerbell join the family via `iterate_orbit` (template step iterator with burn-in + divergence detection). The new "iterative orbit as polyline" tier sits alongside the closed-form curve and chord-set tiers. Three bundled presets ship with styling tuned for the polyline-connect-iterates render path. Animation `Target` enum gains 12 new entries (a/b/c/d × 3 attractors) so envelopes can sweep coefficients. 123 doctest cases (114 → 123), 23 CTest CLI smoke cases (20 → 23).

Next: **Phase 12 — Polish & release.** README screenshots / animated GIFs, expanded curated preset gallery (see "Nice-to-have polish backlog"), Windows cross-compile, itch.io page. Shortest path to a public 1.0 — all v1.1 generator phases are now closed.

**Deferred from Phase 11:** points/scatter geometry tier. Strange attractors are conventionally rendered as one dot per iterate; current pipeline only has `polylines` + `chords` so we connect consecutive iterates. Result reads as orbital density but has a visible stroke texture on Clifford / de Jong. Adding `GeometryBuffer.points` (1×1 px in raylib, tiny `<circle>` in SVG) would give the canonical scatter look — ~150 lines across `geometry_buffer.hpp`, both renderers, and the factory. Reusable for a future phyllotaxis-points mode.

**Deferred from Phase 8:** `Keyframed` envelope (arbitrary control points — needs a 2D curve-editor widget, separate UX project); preset-borne animation (`AnimationSpec` lives only in `AppState`, not serialised — adding it would bump the preset schema again).

**Deferred from Phase 9 Stage B** (not in any committed phase): full hybrid-mode generator (`HybridGenerator(curve, N, k)` — modular chord rule on any `ParametricCurve`) plus `MaurerRose` as its special case on rose. The concrete chord-set special cases (`polygon_chord`, `phyllotaxis_chord`) cover the user-visible string-art family without the nested-generator JSON refactor general hybrid mode would require.

**Deferred from Phase 10:** canvas-drag editing for `LinearEnvelope` endpoints (sliders work, drag would be a UX upgrade).

## Invariants

These rules MUST hold across all phases. Violating any is a bug.

1. **`core/` has zero raylib dependency.** It produces geometric primitives only. The SVG renderer, CLI tool, and tests must build without ever pulling raylib in. If raylib leaks into `core/`, the layering is broken.
2. **Math precision is `double` throughout `core/`.** Cast to `float` only at the GPU boundary inside `render/raylib_renderer.cpp`. Trig accumulates over many chords; the precision matters.
3. **Coordinate convention: math-up (+y up, origin centred) in `core/`.** Y-flip happens inside renderers only.
4. **On-demand rendering, not per-frame.** Geometry only regenerates when the dirty flag is set. Idle CPU/GPU when no parameter is moving. See ARCHITECTURE.md §5.4 for the full model.
5. **Determinism.** Same preset → byte-identical SVG. No time-dependent state without explicit seeding.
6. **Headless capability.** Every feature must work from `cli/` without a window.

## Build & test commands

```bash
# Debug build with tests
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure

# Release build
cmake -B build-release -DCMAKE_BUILD_TYPE=Release
cmake --build build-release -j

# Run the app
./build/app/caustic

# Run the CLI
./build/cli/caustic-cli --help
```

## Conventions

- **C++ style:** roughly Google C++ Style with 4-space indent. `snake_case` for functions and variables, `PascalCase` for types and concepts, `UPPER_SNAKE_CASE` for constants.
- **Header layout:** one class per header where reasonable. `.hpp` for headers, `.cpp` for sources. Public headers in `core/include/caustic/...` use angle brackets (`#include <caustic/vec2.hpp>`); internal-only headers use quotes.
- **Include order:** standard library → third-party → caustic, alphabetised within each group.
- **Commit messages:** imperative mood, scope prefix where useful — e.g. `core: add Vec2 type`, `app: bind k-slider to dirty flag`, `docs: refresh CLAUDE.md`. One topic per commit. Doc-only updates committed separately from code.
- **Tests:** doctest, one test file per source file in `tests/`, named `test_<unit>.cpp`.

## Pitfalls

Captured from architecture review — do not relearn these the hard way.

- **rlImGui version drift.** rlImGui is community-maintained and pinned to specific raylib versions. Always check rlImGui's README for the supported raylib version before bumping either dependency.
- **`SetTargetFPS(60)` does NOT skip the geometry pass.** It limits the render loop, but the dirty-flag check is what gates regeneration. Don't conflate them.
- **raylib's `DrawLine` is unfiltered.** For dense chord patterns, anti-aliasing requires either offscreen high-res `RenderTexture` downsampling or `GL_LINE_SMOOTH` (driver-dependent). Address in Phase 1 if aliasing is visible.
- **Hypotrochoid drift.** Sample by closed-form `t` from the analytic period; never integrate over many revolutions — drift accumulates and the curve will visibly fail to close.
- **`ImGui::IsItemActive()` vs `IsItemHovered()`.** Active = currently being dragged. Hovered = mouse over but not necessarily clicked. The drag-time coarse preview tier uses `IsItemActive()`.
- **Scroll-wheel-on-canvas vs scroll-wheel-on-slider.** Both are scroll inputs. Disambiguate by checking which widget is hovered before consuming the scroll delta.

## Out of scope

Do NOT add the following without explicit confirmation:

- 3D / spherical / hyperbolic variants
- Audio reactivity
- Physical string simulation (tension, gravity)
- G-code / plotter direct output (post-process the SVG instead)
- Network features
- Multi-window / docking layouts (single fixed three-pane layout for v1)
- Custom shader effects (stay CPU-side for v1)
- New generators in v1 beyond: modular chord, hypotrochoid, epitrochoid, Lissajous
- L-systems, tilings (Penrose, Truchet) — different paradigm; would shift Caustic's identity beyond envelope/roulette curves

Planned for v1.1 (do not lobby back into v1):

- Rose curve, Maurer rose, superformula, phyllotaxis — Phase 9 (parametric curve expansion + hybrid mode + multi-layer)
- Polygon base curve, linear two-segment envelope — Phase 10 (string-art expansion; classical curve-stitching patterns the project is literally named for)
- Strange attractors (Clifford, de Jong, Tinkerbell) — Phase 11 (iterative-orbit infrastructure, seed-based determinism)
