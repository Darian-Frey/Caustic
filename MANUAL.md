# Caustic — User Manual

> **Status:** v1.1 — covers all features through Phase 11
> **Last reviewed:** 2026-05-11

A practical guide to driving Caustic — what each control does, what each generator produces, and how to compose common string-art patterns.

## Contents

- [Starting the app](#starting-the-app)
- [The interface](#the-interface)
- [The 12 generators](#the-12-generators)
- [Style](#style)
- [Camera & canvas](#camera--canvas)
- [Layers & composition](#layers--composition)
- [Presets](#presets)
- [SVG export](#svg-export)
- [Animation](#animation)
- [Headless CLI](#headless-cli)
- [Recipes](#recipes)
- [Common pitfalls](#common-pitfalls)

---

## Starting the app

```bash
./build/app/caustic                                         # GUI
./build/cli/caustic-cli presets/cardioid_classic.json -o out.svg
./build/cli/caustic-cli --help                              # CLI flags
```

First-time files are written under `$XDG_CONFIG_HOME/caustic/` (or `$HOME/.config/caustic/`):

- `presets/` — your saved scenes
- `exports/` — exported SVGs
- `animations/` — baked SVG frame sequences

## The interface

Five panels around a central canvas:

| Panel | Purpose |
|---|---|
| **Parameters** (top-left) | Pick a generator and tune its numbers |
| **Style** (bottom-left) | Colour map, stroke, opacity, background |
| **Layers** (top-right) | Add/remove/reorder layers; per-layer transform; array tools |
| **Presets** (right) | Save, load bundled or user presets, export SVG |
| **Animation** (bottom-right) | Envelope target + bake SVG sequence |

Every parameter slider supports **Ctrl+click to type an exact value**, **scroll-wheel on hover to step**, **Shift = ×10 step**, **Ctrl = ×0.1 step**. Slider hovering over the canvas zooms instead.

Each panel has a **Reset** button to return to its struct defaults — useful for escaping a chaotic corner of parameter space.

## The 12 generators

Three families. Switch with the **Generator** combo at the top of the Parameters panel, or with keys `1`–`4` (modular chord / hypotrochoid / epitrochoid / Lissajous).

### Chord sets

Pairs of points joined by straight lines. The "look" emerges as the optical envelope of many such chords.

**Modular chord** — `N` points around a circle; chord *i* connects point *i* to point `round(k·i) mod N`.

- `k = 2` → cardioid
- `k = 3` → nephroid
- `k = 51` (with `N = 200`) → times-table cusp envelopes
- Non-integer `k` (e.g. `2.5`) → smooth morphs between integer figures

**Polygon chord** — Same modular rule, but points distributed along a regular n-gon's perimeter at constant arc length.

- `n = 3`, `k = 2` → deltoid (curved triangle)
- `n = 6`, `k = 2` → hexagram
- `rotation_rad` rotates the underlying polygon

**Phyllotaxis chord** — Points on the golden-angle disk: `P_i = (√i · cos(i·α), √i · sin(i·α))`. Chord rule applied to those N points.

- `α = 137.508°` (golden angle) → sunflower seed spirals
- Off-golden values produce visually chaotic patterns — most parameter space here is noise.
- **Snap buttons** for `α` (golden, 2π/3, 2π/5, 2π/7, π/2) and `k` (2, 3, 5, 7, 11) jump to the aesthetic sweet spots.

**Linear envelope** — Two line segments A and B with N points each; connect A[i] to B[round(k·i) mod N]. This is the classical "thread and nails" pattern.

The chord rule (`k`) determines the visual character — this is the single most important parameter in Caustic:

| `k` | Segments | Visual |
|---|---|---|
| `+1` | shared apex | Parallel chord lines filling a wedge — looks like a filled triangle |
| `−1` | shared apex | **Parabolic envelope** — chord lines tangent to a parabola (the classic curved string-art look) |
| `−1` | parallel | Sunburst through a focal point — silhouette is the `⋈` bowtie |
| `+1` | parallel | Trapezoidal sweep |

**For the iconic curved string-art look, use `k = −1` with shared apex.** All the bundled LinearEnvelope presets use this.

### Parametric curves

Closed-form `t → (x, y)` sampled to a polyline.

**Hypotrochoid** — Classic Spirograph (inner roulette). Sliders: `R` (outer fixed circle), `r` (rolling circle), `d` (pen distance), `samples`. **`R` and `r` are integer-only** (the curve only closes for integer ratios; non-integer makes broken arcs).

**Epitrochoid** — Spirograph rolling on the outside. Same params; produces cardioid-family figures.

**Lissajous** — `(A·sin(a·t + φ), B·sin(b·t))`. `A`, `B`, `φ` continuous; **`a` and `b` integer-only** (same closure reason).

**Rose (rhodonea)** — `r(θ) = cos(n·θ/d)` mapped to Cartesian. Period `2π·d`.

- `n = 5`, `d = 1` → 5-petal star
- `n = 7`, `d = 3` → 7-petal asymmetric rose
- `n = 2`, `d = 1` → quadrifolium

**Superformula (Gielis)** — 6-parameter (`m`, `n1`, `n2`, `n3`, `a`, `b`) generator that morphs between polygons, stars, and organic shapes. Sweep `m` for polygon → starfish.

### Iterative orbits

`(x_n, y_n) → (x_{n+1}, y_{n+1})` traced as a polyline. The orbit's invariant measure is what you see.

Common controls: `a, b, c, d` (map coefficients), `x0, y0` (initial point), `iterations`, `burn_in` (discarded transient).

**Clifford** — `(sin(a·y) + c·cos(a·x), sin(b·x) + d·cos(b·y))`. Canonical: `(-1.4, 1.6, 1.0, 0.7)`.

**de Jong** — `(sin(a·y) − cos(b·x), sin(c·x) − cos(d·y))`. Canonical: `(1.4, −2.3, 2.4, −2.1)`.

**Tinkerbell** — `(x² − y² + a·x + b·y, 2·x·y + c·x + d·y)`. Canonical: `(0.9, −0.6013, 2.0, 0.5)` from `(−0.72, −0.64)`. Easily diverges if you leave the canonical basin — the orbit then truncates and the UI shows a warning.

## Style

The Style panel applies to whatever layer is selected in the Layers panel. Each chord or polyline segment is coloured by sampling the **colour map** at an **indexer t ∈ [0, 1]**.

**Indexers** — what `t` means for a primitive:

- `chord index` — `i / (N−1)`
- `chord length` — relative to longest segment in the set
- `angle` — segment direction normalised to `[0, 1]`
- `curve t` — same as chord index for our samplers; used for clarity on iterative orbits

**Colour maps**:

- `solid` — one colour
- `linear gradient` — start → end
- `HSV sweep` — hue range with fixed saturation/value
- `diverging` — negative, midpoint, positive (good for chord-length-indexed)

**Stroke** — independent min/max widths interpolated by the **width indexer**. Set min = max for uniform thickness.

**Opacity** — 0–1, applied per segment. For dense chord sets, drop to 0.3–0.6 so overlaps build up naturally.

**Background** — scene-wide; affects all layers.

**cyclic** — for closed curves (rose, Lissajous, modular chord), remaps `t` with a triangle wave so the colour returns to its start at `t = 1`. Hides the seam where the curve wraps around.

## Camera & canvas

- **Middle-click drag** → pan
- **Scroll wheel** on canvas → zoom (cursor-relative)
- **F** or **0** → reset camera
- **F11** → toggle borderless fullscreen

The window is resizable down to 640×400; the offscreen render target reallocates to match, so the figure stays sharp at any size.

## Layers & composition

A scene is an ordered list of layers. Each layer = `(generator, style, transform, visible)`. Renderers iterate the list in order; SVG export emits one `<g>` group per layer (Inkscape-friendly).

**Layer actions**: Add, Duplicate, Remove, Move up/down. The visibility checkbox hides a layer without deleting it.

**Per-layer transform** — applied to the layer's geometry after generation, before rendering:

- `tx`, `ty` — translate
- `rotate` — radians
- `scale` — uniform scale factor
- `mirror x` / `mirror y` — negate x or y

Application order: **Mirror → Scale → Rotate → Translate**.

**Array tools** — one-click stamping that converts a single seed layer into N concrete derived layers. The result is N independently-editable layers (not a special "array layer" — once applied, the tool is finished).

- **Rotational array (N)** — N rotated copies around the origin
- **Grid tile (rows × cols, spacing)** — placed on a regular grid
- **Mirror reflect (axis)** — mirror across X or Y

These are the right tool for symmetric compositions. Far easier than placing N copies by hand. See the [Recipes](#recipes) section for concrete examples.

## Presets

Caustic ships with 17 bundled presets covering every generator and several multi-layer compositions. They're listed in the **Bundled** section of the Presets panel.

**To save your work** — type a name in the "Save" field and click **Save**. The preset is written to `$XDG_CONFIG_HOME/caustic/presets/` as a JSON file. **User** presets appear in the panel below the Bundled list; the **Refresh** button rescans the directory.

**Preset format** — versioned JSON (current schema is v2). v1 presets auto-promote on load. See [SPEC.md §3](SPEC.md) for the schema.

## SVG export

Click **Export SVG** in the Presets panel. Output goes to `$XDG_CONFIG_HOME/caustic/exports/`.

Two modes:

- **Coloured** (default) — preserves your style. One `<line>` per chord segment, one `<line>` per polyline segment. Inkscape opens it cleanly.
- **Plotter mode** (toggle the checkbox) — strips opacity and stroke variation, emits a single colour (default `#000000`), and sorts chord-set chords lexicographically by start point so a pen plotter can travel between them more efficiently. Polylines emit as a single `<polyline>` (one pen-down per curve).

Output is **deterministic** — the same preset produces a byte-identical SVG every time.

## Animation

Animate any single parameter over time and bake to a numbered SVG sequence (compose with FFmpeg etc. for video).

**Target** — what to animate. 28 options: generator coefficients (modular k, hypo d, epi d, Lissajous phi/A/B, phyllotaxis α/k, polygon k/rotation, all attractor a/b/c/d), per-layer transform (rotate, scale, translate x/y), camera zoom.

**Envelope** — how the value changes over `t ∈ [0, 1]`:

- `Static` — constant
- `Linear` — `v0` at t=0 → `v1` at t=1
- `Sine` — `offset + amplitude · sin(2π · frequency · t + phase)`

**Playback** — Play / Pause toggles live animation. The time slider scrubs (pauses playback when dragged). Duration sets how long one full sweep takes.

**Bake SVG sequence** — sets the frame count, name prefix, and writes `name_0000.svg`, `name_0001.svg`, … to `$XDG_CONFIG_HOME/caustic/animations/`. Compose offline with FFmpeg:

```bash
ffmpeg -framerate 30 -i animation_%04d.svg out.mp4   # if your ffmpeg has SVG
# or rasterise first:
for f in animation_*.svg; do rsvg-convert "$f" -o "${f%.svg}.png"; done
ffmpeg -framerate 30 -i animation_%04d.png out.mp4
```

The acceptance demo: modular chord with `k` animated `Linear(2 → 3)` over 60 frames produces a smooth cardioid → nephroid morph.

## Headless CLI

`caustic-cli` renders presets to SVG with no window — useful for batch jobs, CI, scripting.

```bash
caustic-cli preset.json -o out.svg
caustic-cli preset.json -o out.svg --width 2048 --height 2048 --margin 0.08 --plotter
```

Flags:

| Flag | Default | Notes |
|---|---|---|
| `--width N` | `1024` | Output canvas width |
| `--height N` | `1024` | Output canvas height |
| `--margin F` | `0.05` | Fraction of canvas reserved as margin |
| `--plotter` | off | Plotter-mode export (see [SVG export](#svg-export)) |
| `--simplify E` | off | Douglas-Peucker epsilon (parsed; not yet applied) |

Exit codes: `0` success / `1` bad args / `2` preset file not found / `3` preset failed validation / `4` write failed.

## Recipes

Common patterns and how to author them. Most of these have a bundled preset you can crib from.

### Parabolic corner fan — the 1960s classic

A single `LinearEnvelope` with two segments sharing an **apex** and `k = −1`. The chord lines are tangent to a parabolic curve.

```
a_start = apex,   a_end = left_corner
b_start = apex,   b_end = right_corner
N = 30-50, k = -1
```

Preset: `corner_fan.json`.

### Bowtie ⋈

Two parabolic corner fans both anchored at the bowtie's **knot point** — one fan reaches left, the other reaches right. Each lobe is a separate layer.

Presets: `envelope_bowtie.json` (single bowtie, diverging colour map), `bowtie_hourglass.json` (two stacked).

### Hourglass / opposing parabolas

Two parabolic corner fans with apexes on **opposite sides of a shared base** (apex pointing in, base to base). Forms an hourglass / S-curve.

Preset: `swoop_pair.json`.

### Diamond unit (4-fold corner fans)

Four parabolic corner fans with apexes at the diamond's four outer tips, each fanning inward to fill one quadrant.

**Build it interactively**: drop a single corner fan, then click **Rotational array** with `N = 4` in the Layers panel. Four rotated layers appear in one click.

### Hexagram / 6-petal star of fans

Six parabolic corner fans, each at a hexagon vertex.

**Build it interactively**: corner_fan + **Rotational array** with `N = 6`.

### Cardioid / nephroid / times tables

`ModularChord`. `N = 200` is a good default. `k = 2` for cardioid, `k = 3` for nephroid, `k = 51` for the famous "times tables mod 200" cusp pattern.

Presets: `cardioid_classic.json`, `times_tables_51.json`.

### Spirograph

`Hypotrochoid`. Try `R = 5`, `r = 3`, `d = 2`. Push `samples` to 8000+ for the larger-period roulettes.

Preset: `spirograph_classic.json`.

### Strange attractor portrait

Pick Clifford / de Jong / Tinkerbell. Hit the **Reset generator params** button to get the canonical values. Set Style to **HSV sweep** with **Color indexer = curve t** for chronological colouring (orbit start → orbit end). Drop stroke to ~0.15 and opacity to ~0.1 so the orbit reads as a density map rather than a filled blob.

Presets: `clifford_butterfly.json`, `de_jong_classic.json`, `tinkerbell.json`.

### Sunflower

`Phyllotaxis`. Click the **golden** snap button under α. `N = 500`. Sets style to HSV sweep for the colour-by-index look.

Preset: `phyllotaxis_sunflower.json`.

## Common pitfalls

- **LinearEnvelope with `k = 1`** produces parallel chord lines, not a curved parabola. For the classic curved string-art look, use **`k = −1` with shared apex**.
- **Phyllotaxis looks broken** — almost all α values are chaotic. Use the **α-snap** buttons (golden, 2π/n) to find the aesthetic sweet spots.
- **Hypotrochoid non-integer R/r** — the curve won't close cleanly. R and r sliders are integer-only by design.
- **Lissajous non-integer a/b** — same closure issue. The "drag through irrational a/b to watch precession" demo is unavailable until a sample-over-many-revolutions mode is added.
- **Tinkerbell diverged** — the orbit escaped its basin. Reduce iterations or stay near the canonical parameters; the parameter panel shows a hint.
- **Attractor too "muddy"** — drop stroke width to 0.15–0.2 and opacity to 0.1, raise iterations to 80k+. Attractors read better as density than as solid fills.
- **Modular chord at k = 1** — every chord is zero-length (point connects to itself). Try k = 2.
- **Exit segfault** — fixed in v1.1; if it returns, check [BUGS.md](BUGS.md) for the renderer-destructor-after-CloseWindow issue.

## Resources

- [README.md](README.md) — project overview
- [SPEC.md](SPEC.md) — preset JSON schema, CLI exit codes
- [ARCHITECTURE.md](ARCHITECTURE.md) — internal design and rendering model
- [BUGS.md](BUGS.md) — debugging journal
- [ROADMAP.md](ROADMAP.md) — phased build plan and what's deferred
- [CHANGELOG.md](CHANGELOG.md) — version history
