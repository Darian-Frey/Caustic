# Caustic — User Manual

> **Status:** v1.1 + Phase 13.1–13.4 — covers all features through v1.1 polish plus the "Surprise me" randomiser, shareable preset URLs, direct G-code / HPGL plotter output, and image trace → CustomChord
> **Last reviewed:** 2026-06-20

A practical guide to driving Caustic — what each control does, what each generator produces, and how to compose common string-art patterns.

## Contents

- [Starting the app](#starting-the-app)
- [The interface](#the-interface)
- [The 17 generators](#the-17-generators)
- [The CustomChord nail editor](#the-customchord-nail-editor)
- [The LinearEnvelope drag editor](#the-linearenvelope-drag-editor)
- [Style](#style)
- [Camera & canvas](#camera--canvas)
- [Keyboard shortcuts](#keyboard-shortcuts)
- [Layers & composition](#layers--composition)
- [Presets](#presets)
- [Exporting](#exporting)
- [Animation](#animation)
- [Frame bake (video)](#frame-bake-video)
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

- `presets/` — your saved scenes (default location only; native file dialogs let you save anywhere)
- `exports/` — exported SVGs
- `animations/` — baked SVG / PNG / GIF / mp4 sequences

## The interface

IDE-style three-pane layout: **left sidebar | canvas | right sidebar**. The two sidebars are tabbed; the canvas fills the middle.

| Sidebar | Tabs |
|---|---|
| **Left** | **Parameters** (pick a generator and tune its numbers) · **Style** (colour map, stroke, opacity, background) |
| **Right** | **Layers** (add/remove/reorder layers; per-layer transform; array tools) · **Presets** (save, load, thumbnail browser, export to SVG/PNG/JPEG) · **Animation** (envelope target + bake SVG / PNG / GIF / mp4) |

**Resize sidebars** — hover the thin strip between sidebar and canvas; the cursor turns into a ↔ arrow. Drag to resize. Both sidebars clamp to `[180 px, window_width × 0.45]`, so the canvas can't collapse to zero. Default width is 320 px each.

**Slider modifiers** — every parameter slider supports **Ctrl+click to type an exact value**, **scroll-wheel on hover to step**, **Shift = ×10 step**, **Ctrl = ×0.1 step**. Scroll-wheel over the canvas zooms instead.

Each panel has a **Reset** button to return to its struct defaults — useful for escaping a chaotic corner of parameter space.

## The 17 generators

Three families. Switch with the **Generator** combo at the top of the Parameters panel, or with keys `1`–`4` (modular chord / hypotrochoid / epitrochoid / Lissajous).

**Don't know where to start?** Click **Surprise me** directly under the Generator combo. It rolls a fresh parameter set for the active generator from a curated stable-region anchor list (canonical k values for modular chord, classic Spirograph R/r ratios for the roulettes, the golden angle for phyllotaxis, Pickover's well-known attractor coefficients for Clifford / de Jong, parabolic-envelope k=−1 with curated geometries for LinearEnvelope, etc.) — you land on aesthetic islands, never in chaotic noise. Click again for a different one. The button is greyed out for CustomChord because that's a hand-authored layout and shouldn't be wiped by a button click.

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

**Linear envelope** — Two line segments A and B with N points each; connect A[i] to B[round(k·i) mod N]. This is the classical "thread and nails" pattern. **Endpoints are draggable on the canvas** — see [the LinearEnvelope drag editor](#the-linearenvelope-drag-editor).

The chord rule (`k`) determines the visual character — this is the single most important parameter in Caustic:

| `k` | Segments | Visual |
|---|---|---|
| `+1` | shared apex | Parallel chord lines filling a wedge — looks like a filled triangle |
| `−1` | shared apex | **Parabolic envelope** — chord lines tangent to a parabola (the classic curved string-art look) |
| `−1` | parallel | Sunburst through a focal point — silhouette is the `⋈` bowtie |
| `+1` | parallel | Trapezoidal sweep |

**For the iconic curved string-art look, use `k = −1` with shared apex.** All the bundled LinearEnvelope presets use this.

**Lissajous chord** — modular chord rule on N nails sampled around a Lissajous curve. `A`, `B` continuous; `a`, `b` integer for closure (same caveat as plain Lissajous).

**Superformula chord** — modular chord rule on N nails sampled around a Gielis superformula curve. Starfish, sea-anemone, and spiked-rosette patterns.

**Diamond stack** — stacked hourglass/diamond modules with parabolic chord fans at each tip. Controls:

- `modules` — how many diamonds stack vertically (adjacent diamonds share a tip)
- `N` — strings per fan
- `aspect` — `half_waist / half_module_height`
- `rotation` — radians
- `fans` — **Both** (4 fans per module, the default), **Vertical only** (top + bottom apexes — fills the top/bottom triangles), **Horizontal only** (left + right apexes — fills the left/right triangles)

The Vertical/Horizontal split is the standard recipe for two-colour stacked-diamond compositions: one layer per fan set, each independently styled.

**Custom chord** — hand-authored nail-and-chord layout. Place nails by clicking the canvas, connect any two with a chord. See [the CustomChord nail editor](#the-customchord-nail-editor).

### Parametric curves

Closed-form `t → (x, y)` sampled to a polyline.

**Hypotrochoid** — Classic Spirograph (inner roulette). Sliders: `R` (outer fixed circle), `r` (rolling circle), `d` (pen distance), `samples`. **`R` and `r` are integer-only** (the curve only closes for integer ratios; non-integer makes broken arcs).

**Epitrochoid** — Spirograph rolling on the outside. Same params; produces cardioid-family figures.

**Lissajous** — `(A·sin(a·t + φ), B·sin(b·t))`. `A`, `B`, `φ` continuous; **`a` and `b` integer-only** (same closure reason).

**Rose (rhodonea)** — `r(θ) = cos(n·θ/d)` mapped to Cartesian. Period `2π·d`.

- `n = 5`, `d = 1` → 5-petal star
- `n = 7`, `d = 3` → 7-petal asymmetric rose
- `n = 2`, `d = 1` → quadrifolium

**Maurer rose** — sin(n·θ) sampled at coprime angular step `step_deg`. With samples=360 and step=71 the polyline shuffles through every sample and produces a dense fractal "necklace" pattern over the rose curve.

**Superformula (Gielis)** — 6-parameter (`m`, `n1`, `n2`, `n3`, `a`, `b`) generator that morphs between polygons, stars, and organic shapes. Sweep `m` for polygon → starfish.

### Iterative orbits

`(x_n, y_n) → (x_{n+1}, y_{n+1})` rendered as polyline, scatter, or both. The orbit's invariant measure is what you see.

Common controls: `a, b, c, d` (map coefficients), `x0, y0` (initial point), `iterations`, `burn_in` (discarded transient), **`render mode`** (Polyline / Scatter / Both).

- **Polyline** — consecutive iterates connected by line segments. Reads as a continuous trajectory; introduces a stroke texture.
- **Scatter** — one dot per iterate. Canonical strange-attractor look — pure density map.
- **Both** — overlay scatter on top of polyline for emphasis.

**Clifford** — `(sin(a·y) + c·cos(a·x), sin(b·x) + d·cos(b·y))`. Canonical: `(-1.4, 1.6, 1.0, 0.7)`.

**de Jong** — `(sin(a·y) − cos(b·x), sin(c·x) − cos(d·y))`. Canonical: `(1.4, −2.3, 2.4, −2.1)`.

**Tinkerbell** — `(x² − y² + a·x + b·y, 2·x·y + c·x + d·y)`. Canonical: `(0.9, −0.6013, 2.0, 0.5)` from `(−0.72, −0.64)`. Easily diverges if you leave the canonical basin — the orbit then truncates and the UI shows a warning.

## The CustomChord nail editor

For hand-authored patterns the procedural generators can't reach. Six edit modes plus three universal interactions (right-click erase, undo/redo, multi-select).

### Edit modes

Pick a mode from the radio buttons in the Parameters panel.

| Mode | Left-click |
|---|---|
| **off** | (no edit action — left-click pans the canvas) |
| **add nail** | Drop a nail at the world coord under the cursor |
| **add chord** | First click selects a nail, second click on a different nail emits a chord. The new chord adopts the active start/end colours (and width/opacity if overrides are in play) |
| **move nail** | Click + hold on an existing nail, drag to reposition (snap honoured) |
| **recolour chord** | Click on an existing chord to apply the active start/end colours |
| **select** | Click an item to select it (Shift+click toggles / adds). Drag in empty space → rubber-band area select. **Delete** key removes the selection |

### Universal interactions

- **Right-click** — erase the nail or chord under the cursor. Works in every mode (including Off). Nails take priority over chords when both are in range.
- **Ctrl+Z / Ctrl+Y** — undo/redo (50-deep stack). Every edit pushes a snapshot. History clears when you switch layers or generators.
- **Delete** — remove all selected items at once.

### Per-chord colour, width, opacity

The CustomChord layer has optional per-chord override arrays. When non-empty AND the size matches `chords.size()`, each entry replaces the layer-style sample for that chord.

- **Active start / end colour** — colour pickers in the panel. Each new chord placed in *add chord* mode picks up these values. Start = end → solid colour. Different start/end → renderer draws a gradient along the chord.
- **Active width / opacity** — width and opacity sliders. Same rule: new chords adopt the active values.
- **Recolour all** / **Apply width to all** / **Apply opacity to all** — paint every existing chord with the current active values.
- **Recolour selected** — paint only the multi-selected chords.
- **Clear chord colours / widths / opacities** — drop the override arrays so the layer's style colormap and stroke take over again.

### Grid and snap

Toggle **show grid** to draw a faint overlay on the canvas. Toggle **snap to grid** to round newly-placed and moved nails to grid intersections.

- **Grid mode** — Rectangular (axis-aligned lines) or **Polar** (concentric rings + radial spokes from origin)
- **Grid spacing** — world units between rectangular lines / polar rings
- **Polar spokes** — number of equally-spaced rays (polar mode only)
- **Show pin numbers** — toggle the index label drawn above each nail

The grid state (mode, spacing, polar spokes, visible, snap) **is saved with the preset** — so a CustomChord layer built against a specific grid keeps that grid when reloaded, and new nails snap to the same intersections.

### Other panel buttons

- **Delete last nail** / **Delete last chord** — quick undo without entering Select mode
- **Clear all** — wipe nails + chords (undoable)

### Image trace

Drop a PNG / JPEG / BMP and Caustic lays nails on the strongest edges, then connects them with the rule of your choice. This is the angle that distinguishes Caustic from photo-string-art tools: the output is a normal CustomChord layer you can keep editing in the nail editor, not a peg-board build script.

Knobs in the panel (above the **Import image…** button):

- **max nails** (10–500) — hard cap on the number of nails emitted. The stratified pass may produce fewer if many cells fail the edge threshold.
- **grid divisions** (4–32) — image is split into `N × N` cells; one nail per cell at most. Keep `grid divisions ≈ √max nails` to avoid most cells being culled.
- **edge threshold** (0–255) — Sobel magnitude cutoff. Higher = pickier; tune this down if you're getting too few nails from a soft-edged photo.
- **chord rule** — **modular** (chord *i* → `round(k·i) mod N`, classic string-art), **sequential** (chord *i* → *i*+1, edge-polyline trace), or **nearest** (each nail to its *k* nearest neighbours, undirected pairs de-duplicated).
- **modular k** or **nearest k** — appears conditionally depending on the rule.

Pipeline detail (for users debugging an unexpected trace):

1. raylib loads the file. Very large images (> 1024 px on the long side) are auto-downscaled to keep the Sobel pass under a second on a typical CPU; you lose nothing visually because 1024 long-side already has plenty of edge detail.
2. Convert to grayscale (one luminance byte per pixel).
3. Run Sobel `|Gx| + |Gy|` magnitude.
4. Stratified sample: one strongest pixel per grid cell above `edge threshold`.
5. If still more than `max nails`, keep the strongest by magnitude.
6. Stable scan-line sort (top-to-bottom, then left-to-right) so chord rules produce predictable patterns.
7. Pixel coords → math-up world coords in `[-1, 1]` with aspect ratio preserved.
8. Build chord pairs per rule.

The import pushes the current CustomChord layer onto the undo stack first — `Ctrl+Z` rescues a bad trace. The status bar reports `imported N nails / M chords from <filename>`.

## The LinearEnvelope drag editor

When the current layer is LinearEnvelope, the four endpoints (`a0`, `a1`, `b0`, `b1`) appear as draggable handles on the canvas. Click and drag to reposition.

**Handle-to-handle snap** — drag a handle near another handle and the target gets a yellow ring; releasing snaps the dragged handle to the target's exact position. This lets you rejoin a split apex (e.g. corner_fan's `a_start` and `b_start`) without pixel-hunting.

**Grid + snap** — same grid state as the CustomChord editor (rectangular or polar; persisted with the preset). The drag honours snap-to-grid the moment you start moving.

**Undo/redo** — Ctrl+Z / Ctrl+Y. Every endpoint drag pushes one snapshot. History clears on layer/generator switch.

Pan still works in this mode: middle-click drag, Spacebar+drag, or left-click drag on empty space (away from any handle).

## Style

The Style panel applies to whatever layer is selected in the Layers panel. Each chord, polyline segment, or scatter point is coloured by sampling the **colour map** at an **indexer t ∈ [0, 1]**.

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

**Stroke** — independent min/max widths interpolated by the **width indexer**. Set min = max for uniform thickness. For attractors in Scatter mode, the dot radius derives from `stroke.width_min`.

**Opacity** — 0–1, applied per segment. For dense chord sets, drop to 0.3–0.6 so overlaps build up naturally.

**Background** — scene-wide; affects all layers.

**cyclic** — for closed curves (rose, Lissajous, modular chord), remaps `t` with a triangle wave so the colour returns to its start at `t = 1`. Hides the seam where the curve wraps around.

**Note**: CustomChord per-chord overrides (colour / width / opacity) take precedence over the layer-level Style for chords that have an override set. Style still drives any non-overridden chords plus all polylines and scatter points.

## Camera & canvas

Pan modes — choose whichever feels best for your input device:

- **Middle-click drag** → always pans (every generator and edit mode)
- **Spacebar + left-drag** → universal pan (works inside the CustomChord and LinearEnvelope editors too — the escape hatch when left-click is doing something else)
- **Plain left-drag**:
  - On most generators → pans
  - On LinearEnvelope → drags the handle under the cursor; pans if the click missed all 4 handles
  - In CustomChord *add chord* / *move nail* / *recolour chord* → does the mode action when on a target, pans on empty space
  - In CustomChord *add nail* / *select* → places a nail / starts rubber-band; use Spacebar+drag or middle-click drag to pan

Other:

- **Scroll wheel** on canvas → zoom (cursor-relative)
- **F** or **0** → reset camera
- **F11** → toggle borderless fullscreen

The window is resizable down to 640×400; the offscreen render target reallocates to match, so the figure stays sharp at any size.

## Keyboard shortcuts

| Keys | Action |
|---|---|
| `Ctrl+S` | Save preset (quick — to `$XDG_CONFIG_HOME/caustic/presets/` using the Save-name field) |
| `Ctrl+Shift+S` | Save preset as… (native file dialog) |
| `Ctrl+O` | Open preset (native file dialog) |
| `Ctrl+E` | Export image… (native file dialog; uses the current format choice — SVG / PNG / JPEG) |
| `Ctrl+N` | New preset (resets to defaults) |
| `Ctrl+Z` / `Ctrl+Y` | Undo / redo (active in CustomChord and LinearEnvelope editors) |
| `Ctrl+Shift+Z` | Redo (alternate) |
| `1` / `2` / `3` / `4` | Quick-switch to ModularChord / Hypotrochoid / Epitrochoid / Lissajous |
| `F` or `0` | Reset camera |
| `F11` | Toggle borderless fullscreen |
| `Delete` | Remove selected nails + chords (CustomChord Select mode) |
| `Spacebar` (held) + drag | Universal pan |

Shortcuts are suppressed when an ImGui text field has focus (so you can type "S" into a filename).

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

Caustic ships with **27 bundled presets** covering every generator and several multi-layer compositions. They're listed in the **Bundled** section of the Presets panel, each with a 96×96 thumbnail preview rendered lazily on first display.

**Quick save** — type a name in the "Save" field and click **Save** (or `Ctrl+S`). The preset is written to `$XDG_CONFIG_HOME/caustic/presets/` as a JSON file. **User** presets appear in the panel below the Bundled list; the **Refresh** button rescans the directory.

**Save as…** — opens a native file dialog so you can save to any location on disk. Same dialog appears for **Open…** (`Ctrl+O`) and **Export as…** (`Ctrl+Shift+E`).

**New** — `Ctrl+N` or the **New** button resets to a default single-layer preset.

**What's persisted**:

- Scene background + every layer (generator + style + transform + visibility)
- Camera state (pan in screen pixels + zoom)
- Editor grid (mode, spacing, polar spokes, visible, snap) — so CustomChord/LinearEnvelope work resumes exactly as you left it

**Preset format** — versioned JSON (current schema is v2). v1 presets auto-promote on load. See [SPEC.md §3](SPEC.md) for the schema.

### Sharing a preset via URL

In the Presets panel next to **Open…**:

- **Copy URL** — encodes the current preset as a single-line `caustic:p1:<base64url>` string and copies it to the system clipboard. The status bar reports the character count. Paste it into any chat, email, forum, or doc that survives ASCII; the bundled gallery encodes to roughly 0.9–5.6 KB per preset.
- **Paste URL** — reads the clipboard. If the contents start with `caustic:p1:` it decodes the embedded preset and replaces the current scene. Whitespace at either end is tolerated, so copy-paste from chat clients (which often append newlines) works without manual trimming.

If the clipboard text is empty, missing the `caustic:p1:` prefix, or fails to decode, your current preset stays put and the status bar tells you which step went wrong. No server, no scheme handler, no account — the URL **is** the preset, encoded compactly enough to share inline.

## Exporting

In the Presets panel (right sidebar), the **Export** section has:

- **format** — choose **SVG (vector)**, **PNG**, **JPEG**, **G-code (plotter)**, or **HPGL (plotter)**
- **filename** — file basename, no extension (the dialog appends the right one)
- **size** — pixel dimensions: width=height. For SVG it's the viewBox; for PNG/JPEG it's the rendered resolution. Auto-hides for plotter formats (they fit-to-page in mm)
- **plotter mode** — SVG-only checkbox (greyed out otherwise); the dedicated G-code / HPGL formats are separate from this
- **plotter page + Z + feedrates** — only appears when G-code or HPGL is selected

Click **Export…** (or press **`Ctrl+E`**) to open the native file picker. The filter and default extension follow the format choice.

### SVG

Vector output. Two sub-modes via the plotter checkbox:

- **Coloured** (default) — preserves your style. One `<line>` per chord segment, one `<line>` per polyline segment, one `<circle>` per scatter point. Inkscape opens it cleanly.
- **Plotter mode** — strips opacity and stroke variation, emits a single colour (default `#000000`), and sorts chord-set chords lexicographically by start point so a pen plotter can travel between them more efficiently. Polylines emit as a single `<polyline>` (one pen-down per curve).

Output is **deterministic** — the same preset produces a byte-identical SVG every time.

### PNG

Lossless raster with alpha. Internally Caustic temporarily resizes the live offscreen canvas to your chosen `size`, redraws the scene at full quality, writes the PNG via raylib's `ExportImage`, restores the canvas size, and refreshes the live view. If your scene background has alpha < 1.0, the PNG preserves it (transparent backgrounds work).

### JPEG

Lossy raster, **no alpha** — the scene background fills the image. Smaller files than PNG; ideal for sharing online. Same render-at-export-size pipeline as PNG.

### G-code (pen plotter)

Grbl-flavour G-code for pen plotters with a Z-axis pen lifter (the most common cheap-plotter setup, e.g. a 3D-printer chassis with a pen head). Header sets mm units, absolute coords, XY plane, units/min feed; footer returns to home and emits `M2`. Per path: `G0` rapid travel to the start point with pen up, `G1` plunge to **pen down Z**, `G1` draws to each subsequent point at the **draw feedrate**, `G0` raises to **pen up Z**.

Knobs in the Export panel when G-code is selected:

- **page width / page height** — the plotter bed dimensions in mm. Drawing fits inside with the standard 5% margin.
- **pen up Z / pen down Z** — Z heights for travel vs. drawing. Defaults: 5 mm up, 0 mm down.
- **travel feed / draw feed / plunge feed** — feedrates in mm/min. Defaults: 6000 / 3000 / 1500.

Output is deterministic (`%.3f` mm precision). Scatter geometry from attractors is skipped — plotters can't draw a true zero-length point.

### HPGL (vintage pen plotter)

Standard HPGL: `IN;` init, `SP<n>;` pen-select, alternating `PU<x>,<y>;` (pen-up move) and `PD<x>,<y>,...;` (pen-down draw to each subsequent point on one statement) per path, `SP0;` + `IN;` footer. Coordinates are integer plotter units at the HP-standard **40 PU/mm**.

Knobs:

- **page width / page height** — the plotter bed dimensions in mm.
- **pen number** — which slot in the pen carousel to select (default 1). Multi-pen support (one layer → one pen) is a future polish.

### Headless plotter output

The desktop GUI is convenient for one-offs but the CLI is the workflow tool — batch every preset in a folder, plug straight into Makefiles, etc. See the [Headless CLI](#headless-cli) section for the full flag table; the short version:

```bash
caustic-cli preset.json -o out.gcode                   # G-code (inferred from .gcode)
caustic-cli preset.json -o out.hpgl                    # HPGL (inferred from .hpgl)
caustic-cli preset.json --format gcode -o out.txt      # explicit format override
caustic-cli preset.json --format gcode --page-width-mm 297 --page-height-mm 210 \
    --pen-up-z 3 --pen-down-z 0 --draw-feedrate 2400 -o a4.gcode
```

## Animation

Animate any single parameter over time and bake to a numbered frame sequence or video.

**Target** — what to animate. **29 options**: generator coefficients (modular `k`, hypo `d`, epi `d`, Lissajous `phi/A/B`, phyllotaxis `α/k`, polygon `k`/rotation, all attractor `a/b/c/d` × 3, diamond-stack `aspect`/rotation), per-layer transform (rotate, scale, translate x/y), camera zoom.

**Envelope** — how the value changes over `t ∈ [0, 1]`:

- `Static` — constant
- `Linear` — `v0` at t=0 → `v1` at t=1
- `Sine` — `offset + amplitude · sin(2π · frequency · t + phase)`
- `Keyframed` — arbitrary `(t, value)` control points with linear interpolation between them. Add / remove rows from the table; values outside the first/last key clamp to the edges

**Playback** — Play / Pause toggles live animation. The time slider scrubs (pauses playback when dragged). Duration sets how long one full sweep takes.

## Frame bake (video)

The Animation panel has three bake outputs. All write into `$XDG_CONFIG_HOME/caustic/animations/<name>/`.

- **Bake SVG sequence** — writes `name_0000.svg`, `name_0001.svg`, … Compose offline with FFmpeg (rasterise first via `rsvg-convert`, then encode).
- **Bake PNG sequence** — writes PNG frames using the live raylib render path. Honours the current canvas size. If **encode mp4 after PNG** is checked, Caustic will spawn `ffmpeg` (must be on `PATH`) and emit `name.mp4` with the H.264 yuv420p baseline that plays anywhere.
- **Bake animated GIF** — encodes a GIF in-process via `msf_gif` (no external tools). Frame interval and bit depth honour the panel sliders.

**Clean up frame files after GIF / mp4 bake** (checkbox, default on) — when a single-file output (GIF, or mp4 from PNG sequence) succeeds, Caustic removes any `<name>_NNNN.svg` / `.png` / `.jpg` intermediates sharing that name prefix in the output folder. This keeps `animations/` from accumulating thousands of frame files across repeated bakes; the GIF / mp4 itself is the deliverable. Turn it off if you want to keep both the frames and the single-file output. The check matches the bake's `_NNNN` pattern exactly, so a file you saved as `animation.svg` (no `_NNNN`) is never touched.

The acceptance demo: modular chord with `k` animated `Linear(2 → 3)` over 60 frames produces a smooth cardioid → nephroid morph.

## Headless CLI

`caustic-cli` renders presets to SVG, G-code, or HPGL with no window — useful for batch jobs, CI, scripting.

```bash
caustic-cli preset.json -o out.svg
caustic-cli preset.json -o out.svg --width 2048 --height 2048 --margin 0.08 --plotter
caustic-cli preset.json -o out.gcode                   # G-code (extension inferred)
caustic-cli preset.json --format hpgl -o pattern.txt   # explicit format override
```

Format selection — `--format` overrides; otherwise the output extension picks:

| Extension | Format |
|---|---|
| `.svg` | SVG |
| `.gcode` / `.nc` / `.gc` | G-code (Grbl flavour) |
| `.hpgl` / `.plt` | HPGL |

Flags by family:

| Flag | Default | Notes |
|---|---|---|
| `--format FMT` | inferred | One of `svg`, `gcode`, `hpgl` |
| `--margin F` | `0.05` | Margin as a fraction of page / canvas (all formats) |
| **SVG only** | | |
| `--width N` | `1024` | Output viewBox width |
| `--height N` | `1024` | Output viewBox height |
| `--plotter` | off | SVG plotter mode (single colour, no opacity, sorted) |
| `--simplify E` | off | Douglas-Peucker epsilon (parsed; not yet applied) |
| **G-code + HPGL** | | |
| `--page-width-mm F` | `200` | Page / bed width in mm |
| `--page-height-mm F` | `200` | Page / bed height in mm |
| **G-code only** | | |
| `--pen-up-z F` | `5.0` | Z height with pen lifted (mm) |
| `--pen-down-z F` | `0.0` | Z height with pen touching (mm) |
| `--travel-feedrate F` | `6000` | Rapid travel feedrate (mm/min) |
| `--draw-feedrate F` | `3000` | Drawing feedrate (mm/min) |
| `--plunge-feedrate F` | `1500` | Z-axis pen up/down feedrate (mm/min) |
| **HPGL only** | | |
| `--pen-number N` | `1` | Pen carousel slot to select |

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

### Two-colour diamond stack

Use two **DiamondStack** layers stacked at the same modules/N/aspect, one with `fans = Vertical only` and the other with `fans = Horizontal only`. Style each in its own colour (classic recipe: white verticals + red horizontals).

Preset: `diamond_stack_two_colour.json`.

### Neon star (4-fold rotational diamond stack)

A DiamondStack layer + **Rotational array N = 4** in the Layers panel.

Preset: `diamond_star_neon.json`.

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

Pick Clifford / de Jong / Tinkerbell. Hit the **Reset generator params** button to get the canonical values. Set **render mode = Scatter** for the canonical density look (Polyline is fine but adds stroke texture). Set Style to **HSV sweep** with **Color indexer = curve t** for chronological colouring (orbit start → orbit end). Stroke width ~1.5px and opacity ~0.45 give a clean scatter; for Polyline mode, drop stroke to ~0.15 and opacity to ~0.1 so the orbit reads as a density map rather than a filled blob.

Presets: `clifford_butterfly.json`, `de_jong_classic.json`, `tinkerbell.json`.

### Sunflower

`Phyllotaxis`. Click the **golden** snap button under α. `N = 500`. Sets style to HSV sweep for the colour-by-index look.

Preset: `phyllotaxis_sunflower.json`.

### Maurer rose

`MaurerRose`. Try `n = 7`, `step_deg = 71`, `samples = 360`. The "necklace" texture comes from the coprime relationship between `step_deg` and 360.

Preset: `maurer_rose_classic.json`.

### Custom hand-authored pattern

1. Pick `CustomChord` as the generator.
2. Turn on **show grid** and **snap to grid** (try Polar mode with 12 spokes and spacing 0.1).
3. Switch to **add nail** mode and click on grid intersections to lay down nails.
4. Switch to **add chord** mode and connect them. Pick start/end colours in the panel — start = end is a solid colour; different colours produce a gradient along the chord.
5. Use Ctrl+Z if you misclick; **move nail** mode to reposition existing nails; **right-click** to erase.
6. Save the preset — the grid settings round-trip, so when you reopen it the nails still align.

### Trace a photo into string art

1. Pick `CustomChord` as the generator.
2. In the **Image trace** section, start with the defaults (`max nails = 100`, `grid divisions = 12`, `edge threshold = 60`, `chord rule = modular`, `modular k = 2`).
3. Click **Import image…** and pick a high-contrast PNG / JPEG. Portraits and silhouettes work best — soft-edged photos may need a lower `edge threshold` (try 30–40).
4. If too few nails appear, lower `edge threshold` or raise `grid divisions`. If too many cluster in one region, raise `grid divisions` to spread them.
5. Try the other rules — `sequential` traces the outline as a polyline (great for line drawings); `nearest k=2` produces a wireframe-like effect.
6. The result is a regular CustomChord layer — switch to **add nail** / **add chord** / **move nail** / **recolour chord** modes and refine by hand. **Ctrl+Z** rescues any bad trace.

The trace finds Sobel edges in pixel space; very large images auto-downscale to 1024 px on the long side to keep the operation snappy.

Preset: `custom_starburst.json`.

## Common pitfalls

- **LinearEnvelope with `k = 1`** produces parallel chord lines, not a curved parabola. For the classic curved string-art look, use **`k = −1` with shared apex**.
- **Phyllotaxis looks broken** — almost all α values are chaotic. Use the **α-snap** buttons (golden, 2π/n) to find the aesthetic sweet spots.
- **Hypotrochoid non-integer R/r** — the curve won't close cleanly. R and r sliders are integer-only by design.
- **Lissajous non-integer a/b** — same closure issue. The "drag through irrational a/b to watch precession" demo is unavailable until a sample-over-many-revolutions mode is added.
- **Tinkerbell diverged** — the orbit escaped its basin. Reduce iterations or stay near the canonical parameters; the parameter panel shows a hint.
- **Attractor too "muddy"** — switch to Scatter render mode, drop stroke width to 0.15–0.5 and opacity to 0.1–0.45, raise iterations to 80k+.
- **Modular chord at k = 1** — every chord is zero-length (point connects to itself). Try k = 2.
- **CustomChord nails won't line up after reload** — the editor grid round-trips with the preset, but only if you saved it after configuring the grid. Re-save the preset with the grid configured and reopen.
- **Left-click pans when I want to edit in CustomChord** — make sure the edit mode is not Off. In edit modes, left-click on a target does the edit action; left-click on empty space pans. Use Spacebar+drag if you want to pan inside an edit mode that consumes empty-space clicks (AddNail / Select).
- **mp4 bake produced no file** — check that `ffmpeg` is on your `PATH`. The PNG sequence still wrote correctly; you can encode it yourself. When mp4 encoding fails, the intermediate PNGs are kept so you can salvage them or retry the encode manually.
- **GIF / mp4 bake deleted my SVG frames** — that's the **clean up frame files after GIF / mp4 bake** checkbox doing its job. It removes per-frame intermediates (`<name>_NNNN.svg/.png/.jpg`) once a single-file output succeeds. Turn it off if you want both. Files without the `_NNNN` suffix are never touched.
- **JPEG export logs "format not supported"** — raylib's image-export support is build-gated. Caustic enables PNG, JPG, and BMP in its `CMakeLists.txt` via `target_compile_definitions(raylib PRIVATE SUPPORT_FILEFORMAT_JPG=1 SUPPORT_FILEFORMAT_BMP=1)`. If you see the warning, raylib was built without those flags — clean-rebuild the raylib target (`cmake --build build --target raylib --clean-first`).
- **Exit segfault** — fixed in v1.1; if it returns, check [BUGS.md](BUGS.md) for the renderer-destructor-after-CloseWindow issue.

## Resources

- [README.md](README.md) — project overview
- [SPEC.md](SPEC.md) — preset JSON schema, CLI exit codes
- [ARCHITECTURE.md](ARCHITECTURE.md) — internal design and rendering model
- [BUGS.md](BUGS.md) — debugging journal
- [ROADMAP.md](ROADMAP.md) — phased build plan and what's deferred
- [CHANGELOG.md](CHANGELOG.md) — version history
