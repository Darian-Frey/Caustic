# CLAUDE.md

## Project

Caustic is a C++20 desktop studio for generative geometric art — chord patterns ("string art") and roulette curves ("spirograph"), with live parameter editing via rlImGui and vector SVG export. Targets a freemium release on itch.io alongside an open-source GitHub repo.

## Current state

**v1.2 feature-complete (Phases 0–11 + v1.1 polish + all of Phase 13, 2026-06-20).** 17 generators across three pipeline tiers (chord sets + parametric curves + iterative orbits, with a scatter render-mode for the attractors). Multi-layer scenes with per-layer transform + array tools (rotational / grid / mirror). Full style system. IDE-style three-pane layout with draggable sidebars (Parameters + Style on the left; Layers + Presets + Animation on the right; canvas in the middle). CustomChord nail editor with six modes, undo/redo, per-chord colour/gradient/width/opacity. LinearEnvelope drag editor with handle-to-handle snap. Editor grid (rectangular or polar) persisted in the preset. Animation system: `Static` / `Linear` / `Sine` / `Keyframed` envelopes × 29 animatable targets. Frame bake: SVG-sequence / PNG-sequence (+ optional ffmpeg mp4) / in-process GIF, with automatic cleanup of per-frame intermediates after a single-file output. Static-image export: SVG (with plotter mode) / PNG / JPEG (raylib's `SUPPORT_FILEFORMAT_JPG` flag enabled at the CMake level). Native file dialogs (tinyfiledialogs). Preset browser with 96×96 thumbnails. Universal pan (middle-drag / Spacebar+drag / smart left-drag). "Surprise me" per-generator randomiser drawing from curated stable-region anchors. AppImage packaging (Linux). Headless `caustic-cli`. CI runs full + headless builds + batch SVG render. **177 doctest cases + 33 CTest CLI smoke cases passing.**

Pinned dependency matrix (BUILD.md is the source of truth): raylib 6.0 (with `SUPPORT_FILEFORMAT_JPG=1` and `SUPPORT_FILEFORMAT_BMP=1` set via `target_compile_definitions` because raylib's `config.h` ships those formats commented out — see BUGS.md), rlImGui `Raylib_6_0`, Dear ImGui v1.92.7, nlohmann/json v3.11.3, doctest v2.4.11, tinyfiledialogs v2.9.3 (native file pickers), msf_gif v2.3 (GIF encoder). rlImGui's `Raylib_*` tags name the matching raylib release — bump in lockstep. mp4 export uses an external `ffmpeg` on `PATH`. CMake project langs are `C CXX` because tinyfiledialogs ships C source.

**Phase 4 deviation worth remembering:** R/r and a/b sliders are integer-only. The architecture's "drag through irrational a/b to watch Lissajous precess" demo is unavailable until someone adds a sample-over-many-revolutions mode + toggle. d, A, B, φ stay continuous (they don't affect closure).

**Phase 5 deviation worth remembering:** `Preset.camera` stores pan as screen pixels (`pan_x_px`, `pan_y_px`) + zoom. SPEC has been updated to match the implementation. Not strictly portable across canvas sizes.

**Phase 6 deviation worth remembering:** Douglas–Peucker simplification is documented in SPEC.md §5 and plumbed through `SvgOptions::simplify_epsilon` (and the `--simplify` CLI flag) but not yet applied. Coloured polyline exports emit one `<line>` per segment — ~4000 lines for the default samples. Plotter-mode chord sorting is lexicographic-by-start-point, not true nearest-neighbour.

## Active task

**Phase 13 complete (5/5 deliverables shipped, 2026-06-19 → 2026-06-20).** v1.2 competitive feature expansion — five small-to-medium items informed by the 2026-06-19 survey of comparable string-art / spirograph / attractor / plotter tools. Test counts across the phase: doctest 159 → 215 (+56); CTest 33 → 36 (+3).

- [x] **Phase 13.1 — "Surprise me" per-generator randomiser** *(complete, 2026-06-19)*. New `core/include/caustic/randomize.hpp` exposes `randomize_generator(spec, rng)` plus 16 per-type `randomize_*` functions, each pulling from a curated stable-region anchor list. CustomChord is the documented no-op (hand-authored layout protection). 18 new doctest cases; total 177 (was 159).
- [x] **Phase 13.2 — Shareable preset URLs** *(complete, 2026-06-20)*. New `core/include/caustic/base64.hpp` (URL-safe alphabet, no padding, tolerates trailing `=`) and `core/include/caustic/preset_url.hpp` (`encode_preset_url` / `decode_preset_url` / `is_preset_url`). Format: single-line `caustic:p1:<base64url-of-compact-JSON>`. UI: **Copy URL** / **Paste URL** buttons next to Open… in the Presets tab; clipboard via raylib. 13 new doctest cases; total 190 (was 177). No compression in v1 — real-world URLs 0.9–5.6 KB across the bundled gallery. No scheme-handler registration; future Wasm port can reuse the payload as `?p=…` query strings.
- [x] **Phase 13.3 — Direct G-code / HPGL output behind plotter mode** *(complete, 2026-06-20)*. New `render/plotter_renderer.{hpp,cpp}` linked into `caustic-render-svg` (no raylib dep, so the CLI gets it for free). `render_gcode` emits Grbl-flavour (`G21`/`G90`/`G17`/`G94` header, `G0` rapid + `G1` plunge-and-draw with pen-up/down Z, `M2` footer); `render_hpgl` emits standard HPGL (`IN;` / `SP<n>;` header, `PU` / `PD` alternation, integer plotter units at 40 PU/mm). `caustic::PlotterOptions` carries shared page-size + margin, G-code Z heights + 3 feedrates, HPGL pen number. **CLI:** `--format <svg|gcode|hpgl>` with extension inference (`.svg` / `.gcode` / `.nc` / `.gc` / `.hpgl` / `.plt`) plus 8 plotter flags. **GUI:** Export-panel format combo extends to 5 entries; pixel-size slider auto-hides for plotter formats; plotter knob sliders surface only for the relevant format. 12 new doctest cases + 3 new CTest cases; totals 202 (was 190) and 36 (was 33).
- [x] **Phase 13.4 — Image trace → CustomChord layer** *(complete, 2026-06-20)*. New `core/include/caustic/image_trace.hpp` (header-only, no raylib dep — tests drive with synthetic buffers). Pipeline: Sobel `|Gx| + |Gy|` magnitude → stratified sampling (one strongest pixel per `grid_divisions × grid_divisions` cell above `edge_threshold`) → clamp to `max_nails` by edge strength → stable scan-line sort → pixel→math-up world coords in `[-1, 1]` aspect-preserved → chord rule dispatch (`Modular` / `Sequential` / `Nearest`, all undirected pairs de-duped). GUI: **Image trace** section in the CustomChord param panel — 4 sliders + rule combo + **Import image…** button. raylib `LoadImage` handles PNG/JPEG/BMP; runaway-large images auto-downscale to 1024 px long-side; the result pushes through the existing CustomChord undo stack so `Ctrl+Z` rescues a bad trace. 13 new doctest cases; total 215 (was 202). 36 CTest unchanged.
- [x] **Phase 13.5 — Visual timeline editor for `Keyframed` envelope** *(complete, 2026-06-20)*. New `keyframed_timeline_editor` widget in `app/main.cpp` replaces the table-of-rows UI. ~220-tall 2D curve view: t on x (0..1), value on y in user-chosen `[y_min, y_max]`. Background + grid + zero-axis + vertical playback indicator at `anim.current_t`. Piecewise-linear curve through keys + faded held-value pre-roll and tail showing the edge-clamp. Left-click + drag a key to move it (clamped to `(prev.t + ε, next.t − ε)`); left-click empty canvas adds a key + enters drag; right-click deletes (≥ 2 keys). Optional snap (t at 1/16, value at 1/20 of Y range). Aux: y_min / y_max sliders, snap checkbox, **Auto-fit Y**, **Reset keys**. Tooltip on hovered/dragged key. **The `Keyframed { vector<pair<t, value>> }` data structure is unchanged** — pure UX swap. Test counts unchanged (existing `test_envelope.cpp` Keyframed coverage isn't affected; the editor is pure UI). Closes the longest-deferred Phase 8 item.

**Next: Phase 12 — Polish & release** for public 1.0. With all of Phase 13 closed, only the release-blockers remain: README screenshots / animated GIFs, license decision + `LICENSE` file, Windows mingw-w64 cross-compile, itch.io page, optional emscripten web build. The AppImage packaging skeleton already shipped, so Linux distribution is solved — Phase 12's open items are documentation, licensing, and Windows builds.

**Deferred (older items still open):**

- **Preset-borne animation** — serialise `AnimationSpec` into the preset so a saved scene replays its animation on load. Carried over from Phase 8. Would bump the preset schema to v3.
- **General `HybridGenerator(curve, N, k)`** — nested-`ParametricCurve` JSON encoding. Out of scope: `MaurerRose`, `LissajousChord`, `SuperformulaChord`, `PolygonChord`, `PhyllotaxisChord` cover the chord-pattern family as concrete special cases.
- **Curated stable-point preset libraries per generator** (the "60% of target gallery" entry in ROADMAP) — pairs naturally with the Phase 13.1 Surprise-me anchors.
- **First-run tutorial overlay** and **stylus pressure sensitivity in CustomChord** — recently added to the Nice-to-have backlog from the 2026-06-19 survey.

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
