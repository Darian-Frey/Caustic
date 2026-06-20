# Roadmap

Caustic's phased development plan. Each phase ends with a runnable, demonstrable artefact. Phases are append-only — completed phases stay in the document with `*(complete, YYYY-MM-DD)*` markers so the historical sequence is preserved.

Ordering is firm but not absolute: dependencies between phases (especially Phase 0 → 1, and Phase 4 depending on 1–3) are real, but cosmetic re-ordering within v1.1 is fine.

**Current state (2026-05-18):** Phases 0–11 closed plus a v1.1 polish batch (generators 13–17, full CustomChord editor, scatter attractors, LinearEnvelope drag editor, Keyframed envelope, PNG/GIF/mp4 bake, keyboard shortcuts, polar grid with preset persistence, per-chord stroke overrides, preset thumbnails, universal pan). Only **Phase 12 — Polish & release** remains for a public 1.0.

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

## Phase 8 — Animation system *(complete, 2026-05-11; Keyframed envelope landed 2026-05-18 in v1.1 polish)*

**Goal:** Parameter envelopes over time, frame export.
**Status:** Complete
**Deliverables:**

- [x] `Envelope` variant: `Static`, `Linear`, `Sine`, **`Keyframed`** (Keyframed shipped in v1.1 polish as a table-of-rows editor — `vector<pair<t, value>>` with linear interpolation between adjacent keys and edge clamp)
- [x] Animation panel with scrub control, play/pause, duration, frame count
- [x] Per-target envelope editor — `Target` enum dispatches to generator/layer/camera fields via `write_target`. **29 animatable parameters** (modular k, hypo d, epi d, Lissajous phi/A/B, phyllotaxis α/k, polygon chord k/rotation, all attractor a/b/c/d × 3, diamond-stack aspect/rotation, layer rotate/scale/translateX/translateY, camera zoom)
- [x] "Bake to SVG sequence" — writes numbered SVGs (`name_0000.svg` … `name_NNNN.svg`) to `$XDG_CONFIG_HOME/caustic/animations/`. **PNG sequence + optional ffmpeg-mp4 and in-process GIF bake** shipped in v1.1 polish.

**Acceptance:** A modular chord with `k` animated `Linear(2.0 → 3.0)` over 60 frames bakes to 60 SVGs that, when composed, show smooth cardioid → nephroid morph. Verified by `test_envelope.cpp` (18 cases incl. Keyframed coverage).

**Deferred:** preset-borne animation (current `AnimationSpec` lives only in `AppState`, not serialised — adding it would bump the preset schema again).

---

## Phase 9 — Parametric curve expansion + hybrid mode + multi-layer scenes *(complete, 2026-05-18)*

**Goal:** Multi-layer scene infrastructure with per-layer transforms and array tools (the load-bearing dependency for the symmetric, tiled, and radially-arrayed compositions the user's reference images keep returning to), plus four new parametric curves and the hybrid-mode generator.
**Status:** Complete (Stage A closed 2026-05-10 with Phase 10; Stage B's new curves and chord-pattern hybrids landed across v1.1 polish 2026-05-11 → 2026-05-18)
**Deliverables:**

**Scene + transforms + arrays** (foundation — Stage A, complete 2026-05-10):

- [x] `caustic::Scene` replaces single-generator `Preset`: scene = ordered list of `Layer { GeneratorSpec, StyleSpec, LayerTransform, visible }`. Preset version bumps to 2; v1 presets auto-promote on load (`style.background` migrates to `scene.background`). 5 bundled presets migrated to on-disk v2 format.
- [x] `LayerTransform { translate, rotate_rad, scale, mirror_x, mirror_y }` applied to each layer's geometry after generation, before rendering. `apply()` math is Mirror → Scale → Rotate → Translate.
- [x] Layer-management UI: list with add / remove / reorder / visibility toggle / duplicate, plus per-layer transform sliders + name input.
- [x] Array tools (free functions in `caustic/array_tools.hpp`): `rotational_array(layer, N, center)`, `grid_tile(layer, rows, cols, spacing)`, `mirror_reflect(layer, axis)`. Triggered from UI buttons; "Apply" replaces the selected layer with N concrete derived copies (each independently editable afterward).
- [x] Both renderers (`SvgRenderer`, `RaylibRenderer`) iterate the scene's layers and apply each layer's transform; SVG emits one `<g>` per layer (Inkscape-friendly). 74 doctest cases (Stage A added 8 array-tool tests + multi-layer SVG tests).

**New curves and hybrid mode** (content — Stage B, complete across v1.1 polish):

- [x] `RoseCurve(n, d)` — `r = cos(n·θ/d)` parametric curve
- [x] `MaurerRose` — landed 2026-05-18 as a concrete generator (`maurer_rose(n, step_deg, samples)`) rather than a hybrid-mode special case; sin(n·θ) sampled at coprime step produces the dense fractal "necklace" texture
- [x] `SuperformulaCurve` (Gielis) — single 6-parameter equation generating stars, flowers, polygons, organic forms
- [x] `PhyllotaxisGenerator` — golden-angle point disk; chord mode shipped (scatter-only `points` mode arrived later as part of the attractor scatter tier in v1.1 polish)
- [x] **Chord-pattern hybrids** chosen over general `HybridGenerator(curve, N, k)` — three concrete chord-set generators ship the user-visible string-art family without the nested-`ParametricCurve` JSON refactor: `LissajousChord`, `SuperformulaChord`, and `MaurerRose`. Plus `PolygonChord` (Phase 10) and `PhyllotaxisChord` cover the polygon and disc cases. General hybrid mode stays out of scope.

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

**Deferred polish closed in v1.1:** canvas-drag endpoint editing for `LinearEnvelope` shipped 2026-05-18 — the four endpoints render as draggable canvas handles with handle-to-handle snap and grid honoured. General hybrid mode (`HybridGenerator(curve, N, k)`) stays out of scope; `polygon_chord`, `phyllotaxis_chord`, `maurer_rose`, `lissajous_chord`, and `superformula_chord` are the concrete special cases that cover the user-visible string-art family without the nested-generator JSON refactor.

---

## Phase 11 — Strange attractors *(complete, 2026-05-11)*

**Goal:** Iterative-orbit generator family — Clifford, de Jong, Tinkerbell. New "trace N iterations as polyline" pipeline tier alongside the closed-form curve and chord-set tiers.
**Status:** Complete
**Deliverables:**

- [x] `iterate_orbit<Step>` template — generic step-function iterator with explicit `(x0, y0)`, burn-in, and iteration count
- [x] `clifford_orbit`, `de_jong_orbit`, `tinkerbell_orbit` step functions
- [x] Burn-in (discard first M iterations) + sample N to a polyline
- [x] Divergence detection: `|x|` or `|y| > 1e6` or non-finite values truncate the orbit and set `AttractorOrbit.diverged`; UI shows a hint for Tinkerbell. The renderer's existing fit-to-content scaling handles bounding box automatically, so no separate coarse pre-pass was needed.
- [x] Chronological orbit colouring via the existing `CurveT` indexer (i/(N-1) on the polyline is the iteration index). No new indexer enum value introduced.
- [x] Determinism: `(x0, y0)` and parameters carried in the preset; tested via `test_attractors.cpp` — two consecutive Clifford orbits with the same params produce byte-identical point sequences. Cross-architecture parity not guaranteed for iterative `double` math (documented).
- [x] Three bundled presets: `clifford_butterfly`, `de_jong_classic`, `tinkerbell`. Styling tuned (width 0.15–0.2, opacity 0.10–0.25, iterations 20k–80k) so the polyline-connect-iterates render reads as orbital density rather than a filled blob.
- [x] Animation Target enum gains 12 new entries (a/b/c/d for each attractor) so envelopes can drive coefficient sweeps.

**Acceptance:** Clifford with `(a, b, c, d) = (-1.4, 1.6, 1.0, 0.7)` produces the canonical butterfly. De Jong with `(1.4, -2.3, 2.4, -2.1)` produces the dual-wing classic. Tinkerbell with `(0.9, -0.6013, 2.0, 0.5)` from `(-0.72, -0.64)` stays in basin for 20k iterations. 123 doctest cases (114 → 123: +9 attractor cases), 23 CTest CLI smoke cases (20 → 23: +3 attractor presets).

**Deferred polish closed in v1.1:** points/scatter geometry tier shipped 2026-05-18 — new `GeometryBuffer.points` vector parallel to polylines and chords, new `AttractorRenderMode { Polyline, Scatter, Both }` enum on each attractor params struct, raylib renderer draws `DrawCircleV` per point, SVG renderer emits `<circle>` per point. Bundled attractor presets switched to Scatter mode at ~1.5px / opacity 0.45 for the canonical scatter-plot look.

---

## v1.1 polish — generator expansion + UX work *(complete, 2026-05-11 → 2026-05-18)*

**Goal:** Close out v1.1 by filling in the deferred items from Phases 8–11 and reaching feature parity with the reference-image string-art set the user kept showing.
**Status:** Complete — five sub-batches across seven commits between Phase 11 close (2026-05-11) and the documentation refresh (2026-05-18).
**Deliverables:**

**Generators 13–17 — string-art expansion:**

- [x] **DiamondStack** (13th) — stacked hourglass / diamond modules with parabolic chord fans at each tip; `DiamondStackFans { Both, Vertical, Horizontal }` enum gives the two-colour stacked-diamond recipe one layer per fan set. Animation targets `aspect` and `rotation` added.
- [x] **CustomChord** (14th) — hand-authored nail-and-chord layout with a full editor (see below). The escape hatch for patterns the procedural generators can't reach.
- [x] **MaurerRose** (15th) — sin(n·θ) at coprime angular step; dense fractal necklace.
- [x] **LissajousChord** (16th) — modular chord rule on N nails sampled around a Lissajous curve.
- [x] **SuperformulaChord** (17th) — modular chord rule on N nails sampled around a Gielis superformula curve.

**CustomChord editor punch list** (8f7165d):

- [x] Six edit modes: Off, AddNail, AddChord, MoveNail, RecolourChord, Select
- [x] Mode-independent right-click erases the nail or chord under the cursor (nails take priority)
- [x] 50-deep snapshot undo/redo (Ctrl+Z / Ctrl+Y), cleared on layer/generator switch
- [x] Per-chord start/end colour + 16-subsegment gradient rendering
- [x] AddChord live preview line from the first-clicked nail to the cursor
- [x] Multi-select with rubber-band, Shift+click toggle, bulk Delete and Recolour
- [x] Native file dialogs (tinyfiledialogs) for save-anywhere / open-anywhere / export-anywhere
- [x] Grid + snap (rectangular initially, polar added later in this batch); pin-number toggle; left-click drag pans when edit mode = Off

**Attractor + envelope + bake polish** (bb95c27):

- [x] Points/scatter geometry tier (`GeometryBuffer.points`, `AttractorRenderMode { Polyline, Scatter, Both }`) — closes the Phase 11 deferred
- [x] LinearEnvelope canvas-drag editor with handle-to-handle snap and undo/redo — closes the Phase 10 deferred
- [x] `Keyframed` envelope (vector of `(t, value)` keys with linear interpolation) — closes the Phase 8 deferred
- [x] PNG sequence bake via `RaylibRenderer::write_png`, optional `ffmpeg` mp4 encode after PNG, in-process GIF encoding via single-header `msf_gif`

**Three more chord-pattern generators** (5db9377): MaurerRose, LissajousChord, SuperformulaChord with `test_hybrids.cpp` (6 cases) and three bundled presets — closes the Phase 9 Stage B chord-pattern hybrid item.

**Nice-to-have batch** (dc7f6b1):

- [x] Keyboard shortcuts: `Ctrl+S` / `Ctrl+Shift+S` / `Ctrl+O` / `Ctrl+E` / `Ctrl+Shift+E` / `Ctrl+N` (plus existing 1–4 / F / 0 / F11 / Ctrl+Z/Y / Delete)
- [x] Polar grid mode with `EditorGridMode { Rectangular, Polar }` and `EditorGrid { mode, spacing, polar_spokes, visible, snap }` persisted in the preset (fixes "new nails won't line up after reload")
- [x] Per-chord stroke width and opacity overrides (`GeometryBuffer.chord_width_overrides` / `chord_opacity_overrides`; `CustomChordParams.chord_widths` / `chord_opacities`)
- [x] Preset browser thumbnails (lazy 96×96 `Texture2D` per preset path)
- [x] Universal pan — middle-click drag, Spacebar+drag, and smart left-drag that hits-tests on press and pans on miss

**Documentation:**

- [x] `MANUAL.md` task-oriented user guide (04699fb)
- [x] LinearEnvelope preset gallery rework around k=−1 parabolic envelope (2010d66) — `swoop_pair`, `bowtie_hourglass`, all envelope presets rebuilt around the reversed-index pattern

**Acceptance:** 17 generators bundled across three pipeline tiers. 27 bundled presets covering every generator family plus multi-layer compositions. 159 doctest cases + 33 CTest cases passing. Every Phase 8/9/10/11 deferred-polish item closed except general `HybridGenerator` (explicitly out of scope) and preset-borne `AnimationSpec` (would bump the preset schema).

---

## Phase 12 — Polish & release

**Goal:** Public 1.0 release on GitHub and itch.io.
**Status:** Not started
**Deliverables:**

- [ ] README screenshots and animated GIFs (parameter sweep demos)
- [x] Bundled preset gallery: 20+ curated presets across all generators (27 shipped in v1.1 polish; the curated-stable-points-per-generator item below is the next refinement)
- [ ] **License decision finalised** (MIT vs GPL-3.0) and `LICENSE` file committed
- [ ] Linux native build + Windows build via mingw-w64 cross-compile from the ThinkPad
- [ ] itch.io page at `darian-frey.itch.io/caustic` with 30-second parameter-sweep GIF
- [ ] Pricing decision (recommended: pay-what-you-want, $5–10 suggested floor)
- [ ] Optional: emscripten web build for browser preview

**Acceptance:** A user can download a Windows or Linux binary from itch.io, double-click run it, load a preset, edit parameters live, and export an SVG that opens correctly in Inkscape — all with zero terminal interaction required.

---

## Phase 13 — v1.2 competitive feature expansion *(in progress, started 2026-06-19; 3/5 deliverables shipped)*

**Goal:** Close the most actionable feature gaps surfaced by the 2026 competitive survey of string-art / spirograph / attractor / plotter tools — without changing Caustic's identity as a desktop generative-art instrument (no code-first scripting, no audio reactivity, no 3D, no mobile port; see [README.md](README.md#features) for the existing scope and [CLAUDE.md](CLAUDE.md#out-of-scope) for the firm out-of-scope list). Five small-to-medium deliverables, each independently shippable.
**Status:** In progress (1/5). Informed by 2026-06-19 survey of comparable tools (notes below).
**Deliverables:**

- [x] **"Surprise me" button per generator** *(complete, 2026-06-19)* — `core/include/caustic/randomize.hpp` introduces `randomize_generator(spec, rng)` plus 16 per-type `randomize_*` functions, each drawing from a curated stable-region anchor list (canonical k values for modular chord, classic Spirograph R/r ratios for hypotrochoid, Pickover's attractor catalogue for Clifford / de Jong, narrow ±0.02 jitter for Tinkerbell to stay in basin, four canonical geometries for LinearEnvelope biased 80% to k=−1 for parabolic look, etc.). CustomChord is a documented no-op (hand-authored layouts shouldn't be wiped by a button) — the UI suppresses the button and surfaces a `(disabled — custom layout is hand-authored)` hint. Static `std::mt19937` in the panel seeded once from `random_device`. **18 new doctest cases** in `tests/test_randomize.cpp` cover range bounds, the `r < R` invariant from BUGS.md, basin radius for Tinkerbell, all-three-fan-modes coverage for DiamondStack, dispatcher safety across all 17 types, and seed determinism. 177 doctest cases total (was 159, +18). Pairs naturally with the [Nice-to-have backlog](#nice-to-have-polish-backlog-no-committed-timing)'s stable-point preset libraries — the curated presets become the "anchors" that the randomiser perturbs around.
- [x] **Shareable preset URLs (`caustic:p1:` + base64url JSON)** *(complete, 2026-06-20)*. New `core/include/caustic/base64.hpp` (URL-safe alphabet, no padding, tolerates trailing `=`) and `core/include/caustic/preset_url.hpp` (`encode_preset_url` / `decode_preset_url` / `is_preset_url`). Format: single-line `caustic:p1:<base64url of compact JSON>` — copy-pasteable through any chat / email / forum that survives ASCII. The decoder trims surrounding whitespace and accepts only the exact prefix so future `caustic:p2:` (e.g. compressed) forms get a clean error rather than silent misdecode. UI: **Copy URL** / **Paste URL** buttons next to **Open…** in the Presets tab; clipboard via raylib's `SetClipboardText` / `GetClipboardText`. Three distinct error messages on paste failure (empty / not a preset URL / decode failed). 13 new doctest cases — RFC base64url vectors, URL-safe-alphabet check, binary 0x00–0xFF round-trip, end-to-end preset round-trip, determinism, whitespace tolerance, all-27-bundled-presets round-trip, malformed-input rejection. 190 doctest cases total (was 177, +13). No compression in v1 — real-world URLs land at 0.9 KB (`cardioid_classic`) to 5.6 KB (`four_bowties` 4-layer composition), well within paste range for every major chat platform. No scheme-handler registration: AppImage doesn't have a clean way and the copy/paste flow works without it; future Wasm port can map the same `caustic:p1:` payload to `?p=…` query-string parameters.
- [x] **Direct G-code / HPGL output behind plotter mode** *(complete, 2026-06-20)*. New `render/plotter_renderer.{hpp,cpp}` linked into `caustic-render-svg` (no raylib dep — CLI gets it for free). `caustic::PlotterOptions` carries `width_mm` / `height_mm` / `margin` shared, G-code knobs (`pen_up_z`, `pen_down_z`, `travel_feedrate`, `draw_feedrate`, `plunge_feedrate`), HPGL knob (`pen_number`). `render_gcode` emits Grbl-flavour (`G21` / `G90` / `G17` / `G94` header, `G0` rapid travel with pen-up Z, `G1` plunge-and-draw with pen-down Z, return-to-home + `M2` footer). `render_hpgl` emits standard HPGL (`IN;` / `SP<n>;` header, alternating `PU<x>,<y>;` and `PD<x>,<y>,...;` per path, integer plotter units at 40 PU/mm). Path extraction skips scatter geometry; lex-sort by first point matches svg_renderer's plotter mode. Both formats deterministic. **CLI:** new `--format <svg|gcode|hpgl>` flag with extension inference (`.svg` / `.gcode` / `.nc` / `.gc` / `.hpgl` / `.plt`) + eight plotter flags (`--page-width-mm`, `--page-height-mm`, `--pen-up-z`, `--pen-down-z`, `--travel-feedrate`, `--draw-feedrate`, `--plunge-feedrate`, `--pen-number`). Help text regrouped by family. **GUI:** Export panel's format combo gains "G-code (plotter)" and "HPGL (plotter)" entries; pixel-size slider auto-hides for plotter formats; plotter-knob sliders appear only for the relevant format. **Tests:** 12 new doctest cases + 3 new CTest cases (extension-inference render, explicit `--format hpgl` render, unknown-format-rejection). 202 doctest total (was 190); 36 CTest total (was 33).
- [ ] **Image trace → CustomChord layer** — load a PNG / JPEG, run an edge detector (Canny or similar), sample N points along the strongest edges, and emit them as a CustomChord layer (nails + chord pairs chosen by a configurable rule: every-Nth, nearest-neighbour, all-pairs above a length threshold). Output is a normal Caustic preset the user can keep editing — distinguishes from photo-string-art tools whose output is build instructions for a physical peg board. ~2 days.
- [ ] **Visual timeline editor for Keyframed envelope** — replace the current table-of-rows editor with a 2D curve view: time on the x-axis, value on the y-axis, drag points to add / move / delete, snap-to-grid, optional curve interpolation modes (linear / smooth-step / catmull-rom). Closes the longest-deferred Phase 8 item. The existing `Keyframed { vector<pair<t, value>> }` data structure stays unchanged — this is pure UX. ~2 days.

**Survey notes (2026-06-19).** Caustic's unique combination — chord patterns + roulette curves + parametric envelopes + strange attractors + custom-nail editor + multi-layer scenes + deterministic vector output, all in a desktop GUI — has no direct competitor. The adjacent niches are: photo-to-string-art web tools (different product: physical-build instructions); single-family spirograph apps (web + iOS — narrower than Caustic); strange-attractor visualisers like Chaoscope (specialised; no chord / spiro / envelope); and general creative-coding frameworks (Processing, p5.js, nannou — code-first, no GUI instrument). What competitors *do* have that we don't: randomisation, sharing / galleries, direct plotter output, image-trace input, and timeline-style animation editors — hence the five items above.

**Acceptance:** A new user can hit "Surprise me" on a fresh launch and land on a recognisable canonical pattern (not noise) for each generator; a user can copy a preset URL to a friend and have them paste it into the app to see the same scene; the CLI emits valid G-code that an AxiDraw owner can plot directly without a `vpype` detour; an image dropped onto the canvas creates a CustomChord layer that visibly traces the input's strong edges; the Keyframed envelope's animation panel shows a draggable 2D curve and the underlying data still round-trips through the existing JSON schema.

---

## Nice-to-have polish backlog *(no committed timing)*

The original v1.1-era backlog (keyboard shortcuts, polar grid, per-chord stroke, preset thumbnails, universal pan, scatter attractors, LinearEnvelope drag editor, Keyframed envelope, PNG/GIF/mp4 bake) all shipped in the v1.1 polish batch above. What remains:

- **Curated stable-point preset libraries per generator.** Each generator (modular chord, hypotrochoid, epitrochoid, Lissajous, rose, superformula, phyllotaxis, plus linear-envelope / polygon / strange attractors) has a parameter space dominated by chaotic-looking values with a few "stable" aesthetic islands. Phyllotaxis is the most visible case (drag α off the golden angle and the visual reads as broken), but every generator has analogous sweet spots that a new user has to stumble onto. Bundle 5–10 named presets per generator covering the canonical patterns plus a few well-chosen variants — e.g. cardioid / nephroid / Mathologer-51 for modular chord; classic Spirograph / Tusi-couple / astroid for hypotrochoid; sunflower / pine-cone / 5-spoke / dandelion for phyllotaxis. Snap-buttons in the UI (already added for phyllotaxis α and k) handle the urgent ergonomic case; the curated gallery teaches the parameter space implicitly. 27 presets ship today — call it ~60% of the target gallery.
- Snap-button rows under the most chaotic sliders for the other generators if and when they prove to need them (Lissajous φ, superformula `m`, modular chord `k` integer special cases).
- Stable-region indicator on continuous sliders — compute a "packing quality" metric over parameter space and draw a marker bar under the slider showing where local maxima live. Would auto-surface stable points without curation. Tried and rejected for now as over-engineered relative to snap buttons + curated presets.
- Preset-borne animation — serialise `AnimationSpec` into the preset so a saved scene can replay its animation on load. Carried over from Phase 8 deferred; would bump the preset schema to v3.
- General `HybridGenerator(curve, N, k)` with nested-`ParametricCurve` JSON encoding. Currently out of scope — `MaurerRose`, `LissajousChord`, `SuperformulaChord`, `PolygonChord`, `PhyllotaxisChord` cover the user-visible chord-pattern family as concrete special cases.
- **First-run tutorial overlay** — short interactive walkthrough on first launch that points at the Parameters tab, the canvas, the export button, and the bundled-preset list. MANUAL.md covers the same ground but new users won't read it. Should be skippable + dismissible-forever. Low-effort, medium-impact discoverability item from the 2026-06-19 competitive survey.
- **Stylus / tablet pressure sensitivity in the CustomChord editor** — Wacom and friends report pressure via `WM_POINTER` (Windows), the X Input extension (Linux), or NSEvent's pressure field (macOS). Useful angle for CustomChord: pressure modulates the per-chord stroke width as you place the connecting click. Niche but a clean differentiator for tablet users; would interact well with the recently-added per-chord width overrides.

---

## Beyond v1.2 *(future, no commitment)*

These are noted to keep the door open. None are scoped or scheduled. Direct G-code output and shareable preset URLs both used to live here; both moved up to Phase 13 once the 2026-06-19 competitive survey clarified they're achievable without changing Caustic's identity. The remaining items would each shift the project's identity meaningfully (audio, 3D, web platform), so they stay parked.

- Harmonograph generator
- Lissajous 3D variant (Bowditch curves)
- Audio reactivity (parameter modulation from FFT)
- WebAssembly browser build (with the existing Phase 13 shareable-URL scheme reused for `?preset=...` query strings → instant remix-this-preset links)
