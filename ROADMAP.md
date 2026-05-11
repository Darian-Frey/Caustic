# Roadmap

Caustic's phased development plan. Each phase ends with a runnable, demonstrable artefact. Phases are append-only — completed phases stay in the document with `[Complete]` markers and ISO dates so the historical sequence is preserved.

Ordering is firm but not absolute: dependencies between phases (especially Phase 0 → 1, and Phase 4 depending on 1–3) are real, but cosmetic re-ordering within v1.1 is fine.

---

## Phase 0 — Foundations *(complete, 2026-05-10)*

**Goal:** Buildable workspace with all dependencies bootstrapped and a green test run.
**Status:** Complete
**Deliverables:**

- [x] CMake workspace at repo root with four subprojects (`core/`, `render/`, `app/`, `cli/`)
- [x] `FetchContent` rules for raylib (6.0), rlImGui (`Raylib_6_0`), Dear ImGui (v1.92.7), nlohmann/json (v3.11.3), doctest (v2.4.11)
- [x] `core/include/caustic/vec2.hpp` plus passing doctest cases in `tests/`
- [x] `app/main.cpp` opens a 1280×800 black window with an empty ImGui frame
- [x] `cli/main.cpp` prints `caustic 0.1.0` and exits cleanly
- [x] `.gitignore`, `.editorconfig` (CI stub deferred)

**Acceptance:** `cmake --build build && ctest --test-dir build` green; `./build/app/caustic` opens a window; `./build/cli/caustic-cli` prints version.

---

## Phase 1 — Modular chord MVP

**Goal:** First visible art on screen.
**Status:** Not started
**Deliverables:**

- [ ] `core/include/caustic/curve.hpp` — `ParametricCurve` interface
- [ ] `Circle` parametric curve implementation
- [ ] `ModularChordGenerator(N, k)` producing a `ChordSet`
- [ ] `RaylibRenderer` rendering chords into a `RenderTexture2D` canvas
- [ ] **Dirty-flag rendering loop established** — canvas only redraws on parameter change (ARCHITECTURE.md §5.4)
- [ ] Hardcoded N=200, k=2 demo

**Acceptance:** Running the app shows a cardioid figure; CPU at idle reads near-zero in `top` when the window is not being interacted with.

---

## Phase 2 — Roulettes & Lissajous *(complete, 2026-05-10)*

**Goal:** All four v1 generators implemented and selectable.
**Status:** Complete
**Deliverables:**

- [x] `HypotrochoidCurve`, `EpitrochoidCurve`, `LissajousCurve` in `core/`
- [x] Curve sampler producing polylines (uniform density; adaptive density deferred — uniform is fine for SPEC defaults)
- [x] Generator selector (number-key toggle 1–4 in app — full UI comes in Phase 4)
- [x] Doctest cases for each generator's known special cases (Tusi couple, cardioid cusp, y=x line, bowtie)

**Acceptance:** All four generators render; hypotrochoid R=5, r=3, d=2 produces the 5-petal Spirograph rosette; Lissajous 1:2 produces the bowtie. `GeometryBuffer` introduced as the union of polylines + chords; renderer fit-to-content scaling auto-sizes each generator.

---

## Phase 3 — Style system *(complete, 2026-05-10)*

**Goal:** Visual variety beyond the default solid white.
**Status:** Complete
**Deliverables:**

- [x] `ColorMap` interface + `Solid`, `LinearGradient`, `HsvSweep`, `Diverging`
- [x] `Indexer` enum: `ChordIndex`, `ChordLength`, `Angle`, `CurveT`
- [x] Stroke width modulation with the same indexer family
- [x] Background colour (faint grid overlay deferred — not visually load-bearing for v1)
- [x] `Style.cyclic` flag added (out-of-spec polish): triangle-wave-remaps t for closed curves so colormap and width return to their starting value at t=1, hiding the seam where the curve wraps. Opt-in per `Style`; defaults to off.

**Acceptance:** Modular chord with `HsvSweep` indexed by chord length renders a smooth rainbow gradient; the three closed-curve generators (trochoids, Lissajous) render without visible color/width seams when `cyclic=true`.

---

## Phase 4 — UI (rlImGui) *(complete, 2026-05-10)*

**Goal:** Full live parameter editing — the experience that makes Caustic an instrument rather than a renderer.
**Status:** Complete
**Deliverables:**

- [x] Generator selector dropdown (plus keys 1–4 retained as power-user shortcut)
- [x] Parameter sliders bound to live `AppState`, dirty flag set on every change
- [x] Scroll-wheel-on-hover with Shift (×10) and Ctrl (×0.1) modifiers
- [x] Coarse drag-time preview tier (gated on `ImGui::IsAnyItemActive()`; modular chord N ÷ 4, polyline samples ÷ 2; full-quality regenerate ~1 frame after release)
- [x] Style panel: color map type + per-type controls, color indexer, stroke min/max + width indexer, opacity, background, cyclic toggle
- [x] Camera controls: middle-click pan, cursor-relative scroll-zoom, F or 0 to reset

**Acceptance:** Dragging `k` from 2 → 3 on the modular chord smoothly morphs cardioid → nephroid; dragging `d` on hypotrochoid (R=5, r=3) breathes between hypocycloid and rosette; idle CPU near-zero when no slider is active.

**Deviation from architecture:** R/r and a/b sliders are integer-only. Non-integer (irrational) ratios produce non-closing curves that look like broken arcs at our default sample density, contrary to the user-facing intent of "complete patterns." The architecture's "drag Lissajous a/b through irrational values to watch precession" killer demo is therefore unavailable until a future toggle is added (sample-over-many-revolutions mode).

---

## Phase 5 — Preset system *(complete, 2026-05-10)*

**Goal:** Save and recall configurations.
**Status:** Complete
**Deliverables:**

- [x] JSON serialise / deserialise of full app state via `caustic::Preset` + `nlohmann/json` (`core/include/caustic/preset.hpp` + `preset_io.hpp`)
- [x] Save input + Save button + per-preset Load Selectables in a dedicated Presets panel
- [x] Preset browser panel listing bundled and user presets as names (PngRenderer thumbnails deferred — out of v1 per ARCHITECTURE.md §5.3)
- [x] User preset directory at `$XDG_CONFIG_HOME/caustic/presets` (falls back to `$HOME/.config/caustic/presets`)
- [x] Five bundled presets in `presets/` covering all four generators: `cardioid_classic`, `times_tables_51`, `spirograph_classic`, `cardioid_epi`, `bowtie`
- [x] Out-of-spec polish: fullscreen via F11 (`ToggleBorderlessWindowed`) and resizable window; renderer reallocates its `RenderTexture2D` canvas on resize so native resolution is used at every window size

**Acceptance:** Save a preset to the user dir, restart the app, click it in the Presets panel — visual output identical. Bundled JSON files are human-readable and match the schema in SPEC.md §3. 54 doctest cases passing (12 new: hex round-trip, enum string maps, every colormap variant, version rejection, on-disk save/load, XDG path, bundled-preset smoke parse).

---

## Phase 6 — SVG export *(complete, 2026-05-10)*

**Goal:** Print-quality vector output.
**Status:** Complete
**Deliverables:**

- [x] `SvgRenderer` parallel to `RaylibRenderer`, consuming the same `GeometryBuffer`. `render/CMakeLists.txt` split into `caustic-render-svg` (always built, no raylib) and `caustic-render-raylib` (gated on `CAUSTIC_BUILD_APP`); headless build verified with `CAUSTIC_BUILD_APP=OFF` (no raylib artifacts in the output tree)
- [x] GUI Export button writes to `$XDG_CONFIG_HOME/caustic/exports/` (XDG fallback to `$HOME/.config/caustic/exports/`); output opens correctly in browsers and Inkscape
- [x] Plotter mode toggle: single colour, no `stroke-opacity` attribute, no background `<rect>`, polylines emitted as single `<polyline>` (one pen-down per curve); chord set sorted by start point (lexicographic) — true nearest-neighbour pen-travel reorder deferred as future polish
- [x] Determinism: fixed 6-decimal precision throughout; byte-identical output across runs (verified by doctest)
- [ ] Douglas–Peucker simplification — `SvgOptions::simplify_epsilon` is plumbed through but not yet applied. Deferred until export file size becomes a real concern; SPEC.md still documents the option.

**Acceptance:** Bundled presets export cleanly via the GUI Export button; SVG files render correctly in browsers/Inkscape. 10 new doctest cases cover preamble + viewBox, byte-identical determinism, chord count match, background-rect emission, plotter mode (no rect / no opacity / single colour), plotter polyline mode, per-segment `<line>` in coloured mode, on-disk `write_svg`, fixed-6-decimal precision, and empty-geometry safety. 64 doctest cases passing total.

---

## Phase 7 — CLI tool *(complete, 2026-05-10)*

**Goal:** Headless batch generation.
**Status:** Complete
**Deliverables:**

- [x] `caustic-cli preset.json -o out.svg` interface with SPEC.md §5 exit codes (0 success / 1 args / 2 file-not-found / 3 validation / 4 write-failed)
- [x] `--width`, `--height`, `--margin`, `--plotter`, `--simplify` flags (`--simplify` is plumbed through `SvgOptions::simplify_epsilon` but currently a no-op pending the Phase 6 Douglas–Peucker deferral)
- [x] CLI links only `caustic::core` + `caustic::render-svg`; verified by configuring with `CAUSTIC_BUILD_APP=OFF` and asserting no `libraylib*` artifacts in the build tree
- [x] CTest smoke suite: `cli_version`, `cli_help`, `cli_missing_args` (exit 1), `cli_file_not_found` (exit 2), per-preset render tests for all 5 bundled presets, plus a plotter-mode render
- [x] GitHub Actions workflow at `.github/workflows/ci.yml` with three jobs: full build (raylib + tests), headless build (no raylib + tests + raylib-absence guard), and a batch render that uploads bundled SVGs as a workflow artifact
- [x] Shared geometry/style factories landed in `core/include/caustic/{geometry,style}_factory.hpp` so app and CLI no longer duplicate the spec→runtime construction code

**Acceptance:** Local batch render of all 5 bundled presets completes in **48 ms wall-clock** (well under the 30 s target). Both build modes green, 11/11 CTest cases pass in each.

**v1 milestone:** with Phase 7 done, Caustic has all the v1-scope deliverables — four generators, full style system, rlImGui UI with live editing, preset save/load, SVG export, and a headless CLI. Phases 8–11 are v1.1 expansions (user-prioritized order); Phase 12 is the path to a public 1.0 release.

---

## Phase 8 — Animation system *(complete, 2026-05-11)*

**Goal:** Parameter envelopes over time, frame export.
**Status:** Complete
**Deliverables:**

- [x] `Envelope` variant: `Static`, `Linear`, `Sine` (Keyframed deferred — three smooth envelopes cover the demo space; keyframes would require a timeline-editor widget that's a separate UX project)
- [x] Animation panel with scrub control, play/pause, duration, frame count
- [x] Per-target envelope editor — `Target` enum dispatches to generator/layer/camera fields via `write_target`. 16 animatable parameters (modular k, hypo d, epi d, Lissajous phi/A/B, phyllotaxis α/k, polygon chord k/rotation, layer rotate/scale/translateX/translateY, camera zoom)
- [x] "Bake to SVG sequence" — writes numbered SVGs (`name_0000.svg` … `name_NNNN.svg`) to `$XDG_CONFIG_HOME/caustic/animations/`

**Acceptance:** A modular chord with `k` animated `Linear(2.0 → 3.0)` over 60 frames bakes to 60 SVGs that, when composed, show smooth cardioid → nephroid morph. Verified by `test_envelope.cpp` (12 cases, 114 doctest total).

**Deferred:** `Keyframed` envelope with arbitrary control points (would require a 2D curve-editor widget — separate UX project); preset-borne animation (current `AnimationSpec` lives only in `AppState`, not serialised — adding it would bump the preset schema again).

---

## Phase 9 — Parametric curve expansion + hybrid mode + multi-layer scenes *(v1.1)*

**Goal:** Multi-layer scene infrastructure with per-layer transforms and array tools (the load-bearing dependency for the symmetric, tiled, and radially-arrayed compositions the user's reference images keep returning to), plus four new parametric curves and the hybrid-mode generator.
**Status:** Not started
**Deliverables:**

**Scene + transforms + arrays** (foundation — Stage A, complete 2026-05-10):

- [x] `caustic::Scene` replaces single-generator `Preset`: scene = ordered list of `Layer { GeneratorSpec, StyleSpec, LayerTransform, visible }`. Preset version bumps to 2; v1 presets auto-promote on load (`style.background` migrates to `scene.background`). 5 bundled presets migrated to on-disk v2 format.
- [x] `LayerTransform { translate, rotate_rad, scale, mirror_x, mirror_y }` applied to each layer's geometry after generation, before rendering. `apply()` math is Mirror → Scale → Rotate → Translate.
- [x] Layer-management UI: list with add / remove / reorder / visibility toggle / duplicate, plus per-layer transform sliders + name input.
- [x] Array tools (free functions in `caustic/array_tools.hpp`): `rotational_array(layer, N, center)`, `grid_tile(layer, rows, cols, spacing)`, `mirror_reflect(layer, axis)`. Triggered from UI buttons; "Apply" replaces the selected layer with N concrete derived copies (each independently editable afterward).
- [x] Both renderers (`SvgRenderer`, `RaylibRenderer`) iterate the scene's layers and apply each layer's transform; SVG emits one `<g>` per layer (Inkscape-friendly). 74 doctest cases (Stage A added 8 array-tool tests + multi-layer SVG tests).

**New curves and hybrid mode** (content — Stage B):

- [ ] `RoseCurve(n, d)` — `r = cos(n·θ/d)` parametric curve
- [ ] `MaurerRose` — special case of hybrid mode on rose curve
- [ ] `SuperformulaCurve` (Gielis) — single 6-parameter equation generating stars, flowers, polygons, organic forms
- [ ] `PhyllotaxisGenerator` — golden-angle point disk; `points` mode (scatter) and `chords` mode (modular rule on the disk)
- [ ] `HybridGenerator(curve, N, k)` — modular chord rule applied to N samples of any `ParametricCurve` (including the new ones above)

**Acceptance:** A scene with a faint epitrochoid background and a Maurer rose foreground exports as a single SVG with two `<g>` layers, both correctly styled. Superformula sweeps through `m` produce smooth polygon → starfish morphs; phyllotaxis at α = 137.508° matches a sunflower seed head. Rotational-array of a single `LinearEnvelope` (Phase 10) at N=6 reproduces the radial star-of-bowties pattern from the reference images with one Apply click instead of six manual layer placements.

---

## Phase 10 — String-art expansion *(complete, 2026-05-10)*

**Goal:** Classical curve-stitching patterns — corner-fan parabolic envelopes, polygon-based modular chords, and the composite multi-figure scenes they enable. Caustic is named for the envelope curves these patterns produce; this phase fills the obvious gap in the generator set.
**Status:** Complete
**Deliverables:**

- [x] `PolygonCurve(n_sides, rotation = 0)` — `ParametricCurve` along the perimeter of a regular n-gon, parameterised at constant arc-length. Vertices sit on the unit circle.
- [x] `polygon_chord(n_sides, N, k, rotation_rad)` — chord set: N points along the n-gon perimeter, modular chord rule applied. Deltoid envelope drops out as the n=3, k=2 special case; hexagram is n=6, k=2.
- [x] `linear_envelope(a_start, a_end, b_start, b_end, N, k)` — chord set between two line segments. The classical schoolchild "thread and nails" pattern. Perpendicular segments + k=1 = parabolic corner fan; parallel segments + k=-1 = bowtie.
- [x] App UI: sliders for n-sides / N / k / rotation on `PolygonChord`; `SliderFloat2` widgets for line endpoints on `LinearEnvelope`. Canvas-drag endpoint editing deferred as future polish.
- [x] Six bundled presets covering the canonical patterns: `corner_fan`, `envelope_bowtie`, `deltoid`, `hexagram`, `four_bowties` (4-layer composition), `rgb_triangle` (3-layer composition on a triangle's sides).

**Acceptance:** Deltoid envelope renders cleanly from chord-rule-on-triangle at n=3, k=2. Parabolic corner fan with two perpendicular segments at k=1. Four-bowtie 2×2 grid composes 4 `LinearEnvelope` layers, each independently coloured and positioned. RGB triangle composes 3 `LinearEnvelope` layers on the sides of a triangle with R/G/B corner fans. All 6 presets export to SVG without rendering glitches. 102 doctest cases passing (88 → 102: 8 new polygon + 6 new linear-envelope cases); 20 CTest CLI smoke cases (14 → 20).

**Deferred polish:** canvas-drag endpoint editing for `LinearEnvelope` (would be a meaningful UX win but the sliders work fine). Full hybrid mode (`HybridGenerator(curve, N, k)` for modular chord on any `ParametricCurve`) and Maurer rose still deferred from Phase 9 Stage B — `polygon_chord` and `phyllotaxis_chord` are concrete special cases that cover the user-visible string-art family without the nested-generator JSON refactor that general hybrid mode would require.

---

## Phase 11 — Strange attractors *(v1.1)*

**Goal:** Iterative-orbit generator family — Clifford, de Jong, Tinkerbell. New "trace N iterations as polyline" pipeline tier alongside the closed-form curve and chord-set tiers.
**Status:** Not started
**Deliverables:**

- [ ] `IterativeOrbit` interface — `(x_n, y_n) → (x_{n+1}, y_{n+1})` with explicit `(x_0, y_0)` and iteration count
- [ ] `CliffordAttractor`, `DeJongAttractor`, `TinkerbellAttractor` implementations
- [ ] Burn-in (discard first M iterations) + sample N to a polyline (or scatter set)
- [ ] Bounding box estimated from a coarse pre-pass; degenerate / divergent orbits surface a warning rather than emit broken SVG
- [ ] `by_iteration_index` color indexer to colour orbits chronologically
- [ ] Determinism: `(x_0, y_0)` and parameters carried in the preset; same preset → byte-identical SVG within a single toolchain (cross-architecture parity is not guaranteed for iterative `double` math, documented)

**Acceptance:** Clifford with `(a, b, c, d) = (-1.4, 1.6, 1.0, 0.7)` produces the canonical butterfly attractor; an orbit of 100k iterations renders cleanly via the existing polyline path; same preset across two consecutive runs produces byte-identical SVG.

---

## Phase 12 — Polish & release

**Goal:** Public 1.0 release on GitHub and itch.io.
**Status:** Not started
**Deliverables:**

- [ ] README screenshots and animated GIFs (parameter sweep demos)
- [ ] Bundled preset gallery: 20+ curated presets across all generators
- [ ] **License decision finalised** (MIT vs GPL-3.0) and `LICENSE` file committed
- [ ] Linux native build + Windows build via mingw-w64 cross-compile from the ThinkPad
- [ ] itch.io page at `darian-frey.itch.io/caustic` with 30-second parameter-sweep GIF
- [ ] Pricing decision (recommended: pay-what-you-want, $5–10 suggested floor)
- [ ] Optional: emscripten web build for browser preview

**Acceptance:** A user can download a Windows or Linux binary from itch.io, double-click run it, load a preset, edit parameters live, and export an SVG that opens correctly in Inkscape — all with zero terminal interaction required.

---

## Nice-to-have polish backlog *(no committed timing)*

Smaller polish items that improve the editing experience without warranting their own phase. Pick off as time allows.

- **Curated stable-point preset libraries per generator.** Each generator (modular chord, hypotrochoid, epitrochoid, Lissajous, rose, superformula, phyllotaxis, plus future linear-envelope / polygon / strange attractors) has a parameter space dominated by chaotic-looking values with a few "stable" aesthetic islands. Phyllotaxis is the most visible case (drag α off the golden angle and the visual reads as broken), but every generator has analogous sweet spots that a new user has to stumble onto. Bundle 5–10 named presets per generator covering the canonical patterns plus a few well-chosen variants — e.g. cardioid / nephroid / Mathologer-51 for modular chord; classic Spirograph / Tusi-couple / astroid for hypotrochoid; sunflower / pine-cone / 5-spoke / dandelion for phyllotaxis. Snap-buttons in the UI (already added for phyllotaxis α and k) handle the urgent ergonomic case; the curated gallery teaches the parameter space implicitly.
- Snap-button rows under the most chaotic sliders for the other generators if and when they prove to need them (Lissajous φ, superformula `m`, modular chord `k` integer special cases).
- Stable-region indicator on continuous sliders — compute a "packing quality" metric over parameter space and draw a marker bar under the slider showing where local maxima live. Would auto-surface stable points without curation. Tried and rejected for now as over-engineered relative to snap buttons + curated presets.

---

## Beyond v1.1 *(future, no commitment)*

These are noted to keep the door open. None are scoped or scheduled.

- Harmonograph generator
- Lissajous 3D variant (Bowditch curves)
- Audio reactivity (parameter modulation from FFT)
- Plotter G-code post-processor as a separate companion tool
- WebAssembly version with shareable preset URLs
