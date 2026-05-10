> **Status:** Active
> **Provenance:** Claude (primary architect, pre-implementation)
> **Last reviewed:** 2026-05-10
> **Why this status:** Architecture locked. Name, GUI library, generator set, and rendering model all decided. Ready for Phase 0.

---

# Caustic — Geometric String Art & Spirograph Studio

A C++20 desktop tool for generating mathematical chord patterns ("string art") and roulette curves ("spirograph"), with vector-perfect SVG export and live parameter manipulation. Targets two audiences: artists wanting print-quality output, and the math-curious wanting to play with parameters and see what falls out.

The name "Caustic" comes from optics: a *caustic* is the envelope curve formed where many rays converge after reflection or refraction off a curved surface. This is precisely what string art evokes — the smooth shape that emerges from a dense fan of straight lines is, mathematically, a caustic.

Distribution target: GitHub for source (open licence TBD), itch.io for pre-built binaries and curated preset packs at darian-frey.itch.io.

---

## 1. Scope

**In scope (v1):**

- Four generators: modular chord, hypotrochoid, epitrochoid, Lissajous
- Live parameter editing (sliders, scroll wheel, drag) with on-demand redraw
- Style system: color maps, stroke width modulation, background
- Preset save/load (JSON)
- Vector SVG export (Inkscape-compatible, plotter-friendly mode)
- Headless CLI: read preset JSON → write SVG (no window required)

**Generator selection rationale:** The v1 set deliberately spans three distinct visual languages: angular geometric (modular chord), flowing curves (trochoids), and oscillator grids (Lissajous). Rose curves were considered but cut — trochoids already cover the "petals and loops" aesthetic via their special cases (cardioid, nephroid, limaçon), so rose adds redundancy. Lissajous brings a genuinely different look (rectangular, oscilloscope-y, retro-electronic) for trivial implementation cost.

**Aspirational (v1.1+):**

- Animation system (parameter envelopes over time, frame export)
- Hybrid mode: modular chords on arbitrary parametric curves
- Multi-layer composition (multiple generators in one scene)
- Rose curve + Maurer rose (chord-pattern variant, fits naturally into hybrid mode)
- Superformula (Gielis) — single 6-parameter equation, huge variety from one generator
- Phyllotaxis — golden-angle point disk, point or chord variant
- Strange attractors (Clifford, de Jong, Tinkerbell) — iterative-orbit family
- Harmonograph generator
- Web build via raylib + emscripten

**Out of scope:**

- 3D / spherical / hyperbolic variants
- Audio reactivity
- Physical string simulation (tension, gravity)
- G-code / plotter direct output (post-process the SVG instead)

---

## 2. Architectural Principles

1. **Math kernel is renderer-agnostic.** The core library has zero dependency on raylib. It produces geometric primitives (chords, polylines) that any renderer can consume. This makes SVG export, batch CLI, future ports, and unit testing all trivial.
2. **Vector-first.** Internal representation is exact analytic geometry, never rasterised pixels. Rasterisation happens at the renderer boundary only.
3. **Determinism.** Same preset + same seed → byte-identical SVG. No floating-point drift between runs, no time-dependent randomness without an explicit seed.
4. **Headless capability.** Every feature must work without a GUI. The window is one of three consumers of the math kernel; the others are SVG export and the test harness.

---

## 3. Layered Architecture

```
┌─────────────────────────────────────┐
│ UI Layer                            │  rlImGui (recommended) or raygui
├─────────────────────────────────────┤
│ Application State                   │  preset, animation, undo, camera
├─────────────────────────────────────┤
│ Renderers      RaylibRenderer  ─┐   │
│                SvgRenderer     ─┤   │  parallel implementations of Renderer
│                (PngRenderer)   ─┘   │
├─────────────────────────────────────┤
│ Math Kernel                         │  generators, curves, color maps
│                                     │  ← headless, testable, no raylib
└─────────────────────────────────────┘
```

The `core/` library contains everything below the renderer line. `render/` contains the two renderers. `app/` is the GUI program. `cli/` is the headless tool. All four are CMake subprojects in one workspace.

---

## 4. Math Kernel

### 4.1 Core types

```cpp
struct Vec2 { double x, y; };

// Anything that can be evaluated at parameter t ∈ [0,1].
class ParametricCurve {
public:
    virtual ~ParametricCurve() = default;
    virtual Vec2 evaluate(double t) const = 0;
    virtual std::pair<Vec2, Vec2> bounding_box() const = 0;  // for SVG viewBox
};

struct Chord { Vec2 a, b; double t_along; };  // t_along for color mapping
using ChordSet = std::vector<Chord>;

// Output of any generator: either a polyline (sampled curve) or a chord set.
struct GeometryBuffer {
    std::vector<std::vector<Vec2>> polylines;
    ChordSet chords;
};
```

### 4.2 Generators

**Modular chord** (string art on a circle):

```
P_i = (cos(2π i/N), sin(2π i/N)),  i ∈ [0, N)
chord_i: P_i → P_{round(k·i) mod N}
```

`k` non-integer is allowed and gives smooth morphing between integer values (e.g. `k = 2.5` is halfway between cardioid and nephroid). N typically 50–5000.

**Hypotrochoid** (Spirograph: small circle radius `r` rolls inside big circle radius `R`, pen offset `d` from rolling centre):

```
x(t) = (R - r) cos(t) + d cos((R - r)/r · t)
y(t) = (R - r) sin(t) - d sin((R - r)/r · t)
```

Curve closes after `t ∈ [0, 2π · r / gcd(R, r)]`. Rational R/r → closed; irrational → fills annulus.

**Epitrochoid** (small circle outside big):

```
x(t) = (R + r) cos(t) - d cos((R + r)/r · t)
y(t) = (R + r) sin(t) - d sin((R + r)/r · t)
```

**Lissajous** (two perpendicular sinusoidal oscillators):

```
x(t) = A sin(a t + φ)
y(t) = B sin(b t)
```

Closed iff `a/b` rational. Integer ratios give the classic oscilloscope figures (1:1 = ellipse, 1:2 = bowtie, 2:3 = trefoil-like). Phase `φ` rotates the figure through its precession family. Grid-aligned aesthetic, fundamentally different from the trochoids.

**Hybrid (v1.1):** sample any `ParametricCurve` at N equispaced points, then apply the modular chord rule. Lets you draw chord patterns on roulettes, Lissajous figures, roses, or arbitrary user curves. The Maurer rose (`r = sin(n θ)` sampled at `θ = k·d°` for fixed integer `d`, lines connecting consecutive samples) drops out as a special case.

**Superformula (v1.1):** Gielis's polar-form supershape — a single 6-parameter equation that captures squares, polygons, stars, flowers, and biological forms.

```
r(φ) = ( |cos(m φ / 4) / a|^n2 + |sin(m φ / 4) / b|^n3 )^(-1/n1)
x(φ) = r(φ) cos(φ)
y(φ) = r(φ) sin(φ)
```

`m` controls rotational symmetry (integer → closed at 2π). `n1, n2, n3` control shape exponents; `a, b` are axis scales. Sweep `m` for polygon morphs; sweep `n1` for shape softening.

**Phyllotaxis (v1.1):** golden-angle point disk:

```
P_i = (√i · cos(i · α), √i · sin(i · α)),  α = 2π / φ²  ≈ 137.508°
```

Output is N points, consumed either as a scatter (one dot per point) or as a point set fed to the modular chord rule (`chord_i: P_i → P_{round(k · i) mod N}`). The chord variant — string art on a sunflower — is the natural Caustic flavour.

**Strange attractors (v1.1):** iterative orbits where `(x_{n+1}, y_{n+1}) = f(x_n, y_n)` for fixed `f`. Three v1.1 variants:

- **Clifford:** `x' = sin(a y) + c cos(a x);  y' = sin(b x) + d cos(b y)`
- **de Jong:** `x' = sin(a y) − cos(b x);  y' = sin(c x) − cos(d y)`
- **Tinkerbell:** `x' = x² − y² + a x + b y;  y' = 2 x y + c x + d y`

Burn in M iterations (typically 1000) to escape transient, then sample the next N (typically 50k–500k) into a polyline or scatter set. Bounding box is estimated from a coarse pre-pass since attractors have no analytic extent. Determinism is preserved via explicit `(x_0, y_0)` + parameters in the preset; the existing "same preset → byte-identical SVG" invariant still holds within a single toolchain, but is no longer cross-architecture (a known caveat of iterative `double` pipelines — same one any FP-heavy generator would have).

### 4.3 Color maps

A `ColorMap` is `f: [0,1] → RGBA`. Built-in implementations:

- `Solid(rgba)`
- `LinearGradient(start, end)`
- `HsvSweep(hue_start, hue_end, sat, val)`
- `Diverging(neg, mid, pos)` — useful for signed quantities

Indexing strategies (what gets fed into the color map):

- `byChordIndex`: i / N
- `byChordLength`: |b − a| / max_length
- `byAngle`: atan2(b − a) normalised to [0,1]
- `byCurveT`: parameter value along the source curve (for sampled roulettes)

Color map and indexer are independent and orthogonal — any combination works.

### 4.4 Stroke parameters

```cpp
struct StrokeStyle {
    double width_min = 1.0;
    double width_max = 1.0;            // equal → constant width
    StrokeWidthIndexer width_indexer;  // same indexers as color
    double opacity = 1.0;
};
```

---

## 5. Rendering Layer

### 5.1 RaylibRenderer

Live preview with **on-demand rendering** (see §5.4 for the full rendering model). Uses `rlgl` for batched line drawing — single draw call for an entire ChordSet. Handles:

- Viewport: square, centred, math-up (+y up). Flip happens here only.
- Background fill (or transparent for screenshot capture)
- Camera: pan/zoom, fit-to-content, reset
- Optional additive blend mode for glow effect (alpha summed across overlapping chords)
- Renders into an offscreen `RenderTexture2D` ("the canvas") which is reused frame-to-frame until invalidated. The window blits the canvas every frame; the expensive geometry pass only runs when the canvas is dirty.

### 5.2 SvgRenderer

Headless, deterministic. One `<line>` per chord, one `<polyline>` per sampled curve. Outputs:

- `viewBox` sized to scene bounding box plus configurable margin
- One `<g>` per logical layer (one per generator instance), Inkscape-friendly
- Stroke colour as `#rrggbb`, opacity as separate `stroke-opacity` attribute (some plotters strip alpha)
- Optional **plotter mode**: single colour, no opacity, sorted by start point for minimal pen travel
- Optional pre-export pass: merge consecutive nearly-collinear segments (Douglas-Peucker, ε configurable)

### 5.3 (Future) PngRenderer

Render via raylib to RenderTexture, save with `ExportImage`. Useful for previews in the preset browser. Not v1.

### 5.4 Rendering model: on-demand, not per-frame

Caustic is a tool for editing still images, not a real-time animation. The rendering model reflects that: **the geometry is recomputed and the canvas is redrawn only when something has changed.** When the user is not interacting, CPU and GPU sit idle.

Application state carries a dirty flag and a cached canvas:

```cpp
struct AppState {
    GeneratorParams  params;
    StyleParams      style;
    CameraState      camera;
    bool             dirty = true;       // geometry/canvas needs rebuild
    GeometryBuffer   cached_geometry;    // last computed primitives
    RenderTexture2D  canvas;             // last rendered canvas
};
```

Per-frame loop:

1. ImGui processes input. Any widget that mutates `params`, `style`, or `camera` sets `dirty = true`.
2. If `dirty`: regenerate `cached_geometry` from the kernel, redraw it into `canvas`, clear the flag.
3. Always: blit `canvas` to the window framebuffer, then draw the ImGui overlay on top.

Step 3 is unavoidable (ImGui rebuilds its own state every frame), but it's microseconds. Step 2 is the expensive one and is now gated.

**Drag-time preview tier.** While a slider is actively held (`ImGui::IsItemActive()`), the dirty flag fires every frame, which would still feel sluggish for heavy patterns (N ≥ 20k chords). Mitigation: when any widget reports active, switch the kernel to a coarse mode — chord count divided by 4, polyline samples halved. On widget release, regenerate at full quality once. This is the *only* situation in which Caustic spends frames doing work; outside an active drag, the loop is essentially free.

**Why not pause the event loop entirely.** raylib's `WaitTime()` or polling-based event loops can drop the window to ~5fps when idle, saving more battery still. Considered but rejected for v1: ImGui's tooltip animations and cursor blinking benefit from a steady ~60fps overlay refresh, and the cost of *just the overlay* is negligible. Revisit if power profiling shows otherwise.

**Implication for SVG export.** None — SVG rendering is independent of the live canvas. Export always runs the kernel at full quality and writes vector primitives directly. The canvas is only for screen preview.

---

## 6. UI Layer

**Confirmed: rlImGui (Dear ImGui binding for raylib).** Justification:

- Many parameters across many generators → need collapsible groups, tabs
- Hybrid number-input + slider widgets are essential for parameters with wide dynamic range (N from 3 to 10000)
- Mature, stable, MIT licensed
- Proven track record in creative tools

**Live editing primitives.** Every numeric parameter is bound to one of:

- `SliderFloat` / `SliderInt` — drag to change. Ctrl+click to type exact values.
- `DragFloat` / `DragInt` — pure drag, no track. Better for unbounded params.
- **Scroll wheel while hovering** — wired explicitly via `ImGui::IsItemHovered() + GetIO().MouseWheel`. Convention: scroll = ±1 step, Shift+scroll = ±10, Ctrl+scroll = ±0.1. Works for both integer (N) and continuous (k, R, r, d) parameters.

Any of these mutating a value sets the canvas dirty flag (§5.4). `ImGui::IsItemActive()` during the same frame triggers the coarse preview tier.

**Killer-demo interactions** — these are the experiences the UI must make smooth:

- Drag `k` continuously from 2.0 → 3.0 on a modular chord and watch the cardioid morph into a nephroid.
- Drag `d` (pen offset) on a hypotrochoid and watch the curve breathe between inner hypocycloid and outer rosette.
- Drag the Lissajous frequency ratio through irrational values and watch the figure precess.

Layout sketch:

```
┌──────────────────────────────────────────────────┐
│ Menu Bar:  File  Edit  View  Render  Help        │
├──────────────┬───────────────────────────┬───────┤
│              │                           │       │
│  Generators  │                           │ Style │
│  ─────────   │      Live Preview         │ ───── │
│  [Modular]   │      (raylib canvas)      │ Color │
│  [Hypo]      │                           │ Width │
│  [Epi]       │                           │ Bg    │
│  [Lissajous] │                           │       │
│              │                           │       │
│  Parameters  │                           │       │
│  N    [200 ] │                           │       │
│  k    [2.0 ] │                           │ Export│
│              │                           │ [SVG] │
└──────────────┴───────────────────────────┴───────┘
```

Camera controls: middle-click drag = pan, scroll over canvas = zoom, F = fit, 0 = reset. Scroll-wheel zoom is canvas-only — scrolling over a slider edits the slider.

---

## 7. Preset System

JSON via `nlohmann/json` (header-only, single CMake `FetchContent` line). One preset = one complete scene: generator + parameters + style + camera.

Example:

```json
{
  "version": 1,
  "name": "cardioid-classic",
  "generator": {
    "type": "modular_chord",
    "params": { "N": 200, "k": 2.0 }
  },
  "style": {
    "color_map": { "type": "linear_gradient",
                   "start": "#1a4480", "end": "#f0c050" },
    "color_indexer": "by_chord_length",
    "stroke": { "width_min": 0.5, "width_max": 0.5, "opacity": 0.6 },
    "background": "#0a0a0a"
  },
  "camera": { "centre": [0, 0], "zoom": 1.0 }
}
```

Bundled `presets/` directory ships with the binary and is loaded into a gallery panel on startup. User presets land in `~/.config/[name]/presets/` (XDG on Linux).

---

## 8. Animation System (v1.1)

Each numeric parameter is wrapped in a `Param<T>` variant:

```cpp
template <typename T>
using Param = std::variant<
    Static<T>,            // constant
    Linear<T>,            // (t0, v0) → (t1, v1)
    Sine<T>,              // amplitude, frequency, phase, offset
    Keyframed<T>          // arbitrary keyframe list, cubic-bezier easing
>;
```

A timeline UI scrubs `t ∈ [0, duration]`. Each generator polls `param.evaluate(t)` once per frame. "Bake to SVG sequence" writes one numbered SVG per frame for offline composition into video — simpler and more robust than embedded SMIL animation, which is poorly supported.

---

## 9. Project Structure

```
caustic/
├── CMakeLists.txt              # workspace root
├── core/                       # math kernel, no raylib dependency
│   ├── CMakeLists.txt
│   ├── include/caustic/
│   │   ├── vec2.hpp
│   │   ├── curve.hpp
│   │   ├── chord.hpp
│   │   ├── color.hpp
│   │   ├── stroke.hpp
│   │   └── generators/
│   │       ├── modular_chord.hpp
│   │       ├── hypotrochoid.hpp
│   │       ├── epitrochoid.hpp
│   │       └── lissajous.hpp
│   └── src/...
├── render/
│   ├── CMakeLists.txt
│   ├── raylib_renderer.hpp/.cpp
│   └── svg_renderer.hpp/.cpp
├── app/                        # GUI executable
│   ├── CMakeLists.txt
│   ├── main.cpp
│   ├── ui_panels.cpp
│   └── preset_browser.cpp
├── cli/                        # headless executable
│   ├── CMakeLists.txt
│   └── main.cpp
├── tests/                      # core tests (doctest)
│   └── ...
├── presets/                    # bundled JSON presets
│   ├── cardioid_classic.json
│   ├── nephroid_neon.json
│   └── ...
├── third_party/                # FetchContent fallbacks
├── ARCHITECTURE.md             # this file
├── CLAUDE.md                   # handoff doc (created at end of Phase 0)
└── README.md
```

---

## 10. Phased Build Plan

Each phase is roughly one focused sitting. Every phase ends with a runnable, demonstrable artefact.

**Phase 0 — Foundations**
- CMake workspace, four subprojects
- raylib + rlImGui + nlohmann/json via `FetchContent`
- doctest integrated, one trivial passing test
- `app/` opens a black window. `cli/` prints version. `core/` has Vec2 + a unit test.

**Phase 1 — Modular chord MVP**
- `Circle` parametric curve
- `ModularChordGenerator(N, k)` produces a `ChordSet`
- `RaylibRenderer` draws solid-colour chords into a `RenderTexture2D` canvas
- Dirty-flag rendering loop established (§5.4): canvas redraws only on parameter change
- Hardcoded N=200, k=2 → cardioid visible on screen, idle CPU at rest

**Phase 2 — Roulettes & Lissajous**
- `HypotrochoidCurve`, `EpitrochoidCurve`, `LissajousCurve`
- Curve sampler → polyline render
- All four generators selectable (hardcoded toggle for now)

**Phase 3 — Style system**
- `ColorMap` interface + four implementations
- Stroke width modulation
- Background colour, optional faint grid

**Phase 4 — UI (rlImGui)**
- Generator selector dropdown
- Parameter sliders + drag widgets bound to live state, all setting the dirty flag
- Scroll-wheel-while-hovering for fine parameter control (Shift / Ctrl modifiers)
- Coarse drag-time preview tier active during `IsItemActive()`
- Style panel (color map, stroke, background)
- Camera controls (pan, zoom, fit, reset)

**Phase 5 — Preset system**
- JSON serialise / deserialise full app state
- Save / Load buttons
- Bundled presets loaded on startup, browsable

**Phase 6 — SVG export**
- `SvgRenderer` parallel to `RaylibRenderer`
- Verified in Inkscape (correct viewBox, layers, colours)
- Plotter mode toggle
- Douglas-Peucker simplification pass (optional, off by default)

**Phase 7 — CLI tool**
- `caustic-cli preset.json -o out.svg`
- No window, no raylib (link `core/` + `render/svg_renderer` only)
- Useful for batch generation and CI tests

**Phase 8 — Animation system** *(v1.1)*
- `Param<T>` templates
- Timeline UI with scrub
- Bake-to-SVG-sequence

**Phase 9 — Parametric curve expansion + hybrid mode** *(v1.1)*
- Modular chords on arbitrary `ParametricCurve`
- Rose curve generator + Maurer rose (chord variant)
- Superformula (Gielis) generator
- Phyllotaxis generator (point + chord variants)
- Multi-layer composition (scene = list of generators)

**Phase 10 — Strange attractors** *(v1.1)*
- New `IterativeOrbit` pipeline tier (parallel to closed-form curves and chord sets)
- Clifford, de Jong, Tinkerbell variants
- Burn-in + sample to polyline; coarse bounding-box pre-pass
- Seed `(x_0, y_0)` + parameters in the preset preserve determinism within a toolchain

**Phase 11 — Polish & release**
- README with screenshots and animated GIFs
- Bundled preset gallery (~20 curated)
- GitHub repo with standard header (per Shane's convention)
- Linux + Windows builds via mingw-w64 cross-compile from the ThinkPad
- itch.io page with 30-second parameter-sweep GIF, pay-what-you-want or $5–10
- Optional: emscripten web build

---

## 11. Resolved Decisions

All major architectural decisions are settled. Recorded here for posterity:

1. **Project name: Caustic.** From optics — the envelope curve where many rays converge. Mathematically meaningful (the smooth shapes that emerge from chord patterns *are* caustics in the technical sense), short, evocative, no naming conflicts. Beats Cycloid, Whorl, Heliotrope, Chordweave, Skein, Gyre, Volute, Moiré.
2. **GUI: rlImGui.** Parameter density rules out raygui's flat widget vocabulary. Dear ImGui handles collapsible groups, tabs, and hybrid slider/text widgets, and rlImGui is the standard binding.
3. **Language: C++20.** Rust + egui was considered and would be slightly cleaner architecturally, but C++ familiarity (terra-siege, Vector Gothic, ARCHIVIST) wins on time-to-first-pixel.
4. **v1 generator set: modular chord + hypotrochoid + epitrochoid + Lissajous.** Rose curves cut from v1 — trochoid special cases already cover the petals/loops aesthetic — and moved to Phase 9 alongside the Maurer rose chord variant. Superformula and phyllotaxis added to Phase 9 as additional parametric curves; strange attractors graduated into a dedicated Phase 10 since they need a new iterative-orbit pipeline tier. L-systems and tilings (Penrose, Truchet) considered and explicitly rejected — they would shift Caustic's identity beyond envelope/roulette curves.
5. **Rendering model: on-demand, not per-frame.** Dirty-flag + cached `RenderTexture2D` canvas. Idle CPU/GPU when no parameter is moving. See §5.4.
6. **Math precision: `double` throughout core**, cast to `float` only at the GPU boundary.
7. **Coordinate convention: math-up (+y up, origin centre)** in core; flip in renderers only.
8. **Default blend mode: alpha-over** for v1; additive (glow) as a toggle once §3 style panel exists.
9. **Distribution: GitHub for source + itch.io for binaries** at darian-frey.itch.io. Pay-what-you-want or low single-digit pricing, source MIT or GPL (TBD at Phase 10).

---

## 12. Risks

- **Aliasing at high chord counts.** raylib's basic `DrawLine` is unfiltered. Mitigation: render to higher-resolution offscreen RenderTexture, downsample. Or use `rlgl` with `GL_LINE_SMOOTH` (driver-dependent). Verify early in Phase 1.
- **SVG file size.** 50k chords ≈ 5–10 MB. Mitigation: optional decimation pass at export, or fall back to high-res PNG for preview-only patterns.
- **Numerical drift in deep MOND-style rational R/r.** Hypotrochoids with high gcd(R, r) ratios can drift visibly over thousands of revolutions. Mitigation: sample by closed-form `t` from analytic period, never integrate.
- **Iterative attractor non-determinism across architectures.** Strange attractors (Phase 10) accumulate floating-point error over hundreds of thousands of iterations. Same toolchain → byte-identical SVG; different libm or compiler may diverge in the low decimal places. Mitigation: pin orbit math to `double`, document the caveat, treat the existing determinism invariant as "within one toolchain".
- **Attractor bounding-box surprise.** Some parameter sweeps push orbits to infinity or collapse to a fixed point. Mitigation: coarse pre-pass with iteration budget; if the box is degenerate or unbounded, surface a warning rather than emit broken SVG.
- **Scope creep.** Lissajous, harmonographs, audio reactivity, 3D variants, plotter G-code — all tempting, all defer to v1.1+.
- **GUI binding maintenance.** rlImGui is community-maintained; check compatibility with the chosen raylib version at Phase 0.

---

## 13. Next Actions

All architectural decisions are closed. Begin Phase 0:

1. Create `caustic/` repo on GitHub (Darian-Frey), standard repo header per Shane's convention.
2. CMake workspace with four subprojects: `core/`, `render/`, `app/`, `cli/`.
3. `FetchContent` for raylib, rlImGui, nlohmann/json, doctest. Verify versions compose cleanly.
4. Trivial passing doctest in `core/` (Vec2 construction).
5. `app/` opens a 1280×800 black window with an empty ImGui panel.
6. `cli/` prints `caustic 0.1.0` and exits.

End-of-Phase-0 deliverable: green tests, runnable window, runnable CLI, clean commit. After that, create `CLAUDE.md` handoff doc in repo root mirroring the terra-siege / Nyx-Audio convention.
