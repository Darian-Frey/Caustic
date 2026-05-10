# Roadmap

Caustic's phased development plan. Each phase ends with a runnable, demonstrable artefact. Phases are append-only — completed phases stay in the document with `[Complete]` markers and ISO dates so the historical sequence is preserved.

Ordering is firm but not absolute: dependencies between phases (especially Phase 0 → 1, and Phase 4 depending on 1–3) are real, but cosmetic re-ordering within v1.1 is fine.

---

## Phase 0 — Foundations

**Goal:** Buildable workspace with all dependencies bootstrapped and a green test run.
**Status:** Not started
**Deliverables:**

- [ ] CMake workspace at repo root with four subprojects (`core/`, `render/`, `app/`, `cli/`)
- [ ] `FetchContent` rules for raylib, rlImGui, nlohmann/json, doctest with pinned versions
- [ ] `core/include/caustic/vec2.hpp` plus one passing doctest in `tests/`
- [ ] `app/main.cpp` opens a 1280×800 black window with an empty ImGui frame
- [ ] `cli/main.cpp` prints `caustic 0.1.0` and exits cleanly
- [ ] `.gitignore`, `.editorconfig`, basic CI stub (optional but recommended)

**Acceptance:** `cmake --build build && ctest --test-dir build` is green; `./build/app/caustic` opens a window; `./build/cli/caustic-cli` prints version.

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

## Phase 2 — Roulettes & Lissajous

**Goal:** All four v1 generators implemented and selectable.
**Status:** Not started
**Deliverables:**

- [ ] `HypotrochoidCurve`, `EpitrochoidCurve`, `LissajousCurve` in `core/`
- [ ] Curve sampler producing polylines with adaptive density
- [ ] Generator selector (hardcoded toggle key for now — full UI comes in Phase 4)
- [ ] Each generator covered by a doctest verifying known special cases (e.g. `R=2r, d=r` gives a straight line for hypotrochoid)

**Acceptance:** All four generators render correctly; hypotrochoid with `R=5, r=3, d=2` matches the canonical Spirograph pattern; Lissajous `1:2` produces a bowtie figure.

---

## Phase 3 — Style system

**Goal:** Visual variety beyond the default solid white.
**Status:** Not started
**Deliverables:**

- [ ] `ColorMap` interface + `Solid`, `LinearGradient`, `HsvSweep`, `Diverging`
- [ ] `ColorIndexer`: `byChordIndex`, `byChordLength`, `byAngle`, `byCurveT`
- [ ] Stroke width modulation with the same indexer family
- [ ] Background colour and (optional) faint grid overlay

**Acceptance:** A modular chord with `HsvSweep` indexed by chord length renders with smooth rainbow gradient; documented in a screenshot.

---

## Phase 4 — UI (rlImGui)

**Goal:** Full live parameter editing — the experience that makes Caustic an instrument rather than a renderer.
**Status:** Not started
**Deliverables:**

- [ ] Generator selector dropdown
- [ ] Parameter sliders + `DragFloat`/`DragInt` widgets bound to live state, all setting the dirty flag on change
- [ ] Scroll-wheel-while-hovering for fine parameter control with Shift (×10) and Ctrl (×0.1) modifiers
- [ ] Coarse drag-time preview tier active during `ImGui::IsItemActive()` (ARCHITECTURE.md §5.4)
- [ ] Style panel: color map type, indexer, stroke min/max, opacity, background
- [ ] Camera controls: middle-click pan, scroll-on-canvas zoom, F to fit, 0 to reset

**Acceptance:** Dragging `k` continuously from 2.0 to 3.0 on a modular chord smoothly morphs cardioid into nephroid; dragging `d` on a hypotrochoid breathes between hypocycloid and rosette; idle CPU stays near zero when no widget is active.

---

## Phase 5 — Preset system

**Goal:** Save and recall configurations.
**Status:** Not started
**Deliverables:**

- [ ] JSON serialise / deserialise of full app state (generator + style + camera)
- [ ] Save / Load buttons in File menu
- [ ] Preset browser panel showing bundled presets with thumbnails (or names if PngRenderer not yet implemented)
- [ ] User preset directory at `~/.config/caustic/presets/` (XDG on Linux)
- [ ] At least 5 bundled presets in `presets/` covering each generator

**Acceptance:** Save a preset, restart the app, load it — visual output is identical. Preset JSON is human-readable and matches SPEC.md schema.

---

## Phase 6 — SVG export

**Goal:** Print-quality vector output.
**Status:** Not started
**Deliverables:**

- [ ] `SvgRenderer` parallel to `RaylibRenderer`, consuming the same `GeometryBuffer`
- [ ] Output verified in Inkscape: correct viewBox, layers, colours, opacities
- [ ] Plotter mode toggle: single colour, no opacity, sorted by start point for minimal pen travel
- [ ] Optional Douglas–Peucker simplification pass (off by default, configurable ε)
- [ ] Determinism test: same preset → byte-identical SVG output

**Acceptance:** Open exported SVG in Inkscape and Firefox, both render correctly. Plotter-mode SVG runs cleanly through `vpype` for verification.

---

## Phase 7 — CLI tool

**Goal:** Headless batch generation.
**Status:** Not started
**Deliverables:**

- [ ] `caustic-cli preset.json -o out.svg` interface
- [ ] `--width`, `--height`, `--margin`, `--plotter`, `--simplify` flags
- [ ] CLI links only `core/` + `render/svg_renderer` (no raylib, no rlImGui)
- [ ] CI smoke test: render every bundled preset, verify non-empty valid SVG output

**Acceptance:** `caustic-cli` builds and runs on a headless Linux container with no display server. Batch script renders all 20+ bundled presets in under 30 seconds total.

---

## Phase 8 — Animation system *(v1.1)*

**Goal:** Parameter envelopes over time, frame export.
**Status:** Not started
**Deliverables:**

- [ ] `Param<T>` variant: `Static`, `Linear`, `Sine`, `Keyframed`
- [ ] Timeline UI panel with scrub control
- [ ] Per-parameter envelope editor
- [ ] "Bake to SVG sequence" — writes numbered SVGs for offline composition into video

**Acceptance:** A modular chord with `k` animated `Linear(2.0 → 3.0)` over 60 frames bakes to 60 SVGs that, when composed, show smooth cardioid → nephroid morph.

---

## Phase 9 — Parametric curve expansion + hybrid mode *(v1.1)*

**Goal:** New parametric generators, modular chords on arbitrary curves, multi-layer scenes.
**Status:** Not started
**Deliverables:**

- [ ] `HybridGenerator(curve, N, k)` — modular chord rule applied to N samples of any `ParametricCurve`
- [ ] `RoseCurve` generator (rational `k` parameter)
- [ ] `MaurerRose` as a special case of hybrid mode on rose curve
- [ ] `SuperformulaCurve` (Gielis) — single 6-parameter equation generating stars, flowers, polygons, organic forms
- [ ] `PhyllotaxisGenerator` — golden-angle point disk; `points` mode (scatter) and `chords` mode (modular rule on the disk)
- [ ] Multi-layer scene: scene = ordered list of generators, each with own style
- [ ] UI for adding, removing, reordering, and toggling visibility of layers

**Acceptance:** A scene with a faint epitrochoid background and a Maurer rose foreground exports as a single SVG with two `<g>` layers, both correctly styled. Superformula sweeps through `m` produce smooth polygon → starfish morphs; phyllotaxis at α = 137.508° matches a sunflower seed head.

---

## Phase 10 — Strange attractors *(v1.1)*

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

## Phase 11 — Polish & release

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

## Beyond v1.1 *(future, no commitment)*

These are noted to keep the door open. None are scoped or scheduled.

- Harmonograph generator
- Lissajous 3D variant (Bowditch curves)
- Audio reactivity (parameter modulation from FFT)
- Plotter G-code post-processor as a separate companion tool
- WebAssembly version with shareable preset URLs
