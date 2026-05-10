# Caustic Specification

The authoritative reference for Caustic's data formats and generator mathematics. ARCHITECTURE.md explains *why* the system is shaped the way it is; this document defines *exactly what* the data and parameters are. When the two disagree, this document wins.

---

## 1. Coordinate System

- **Origin:** centre of the canvas
- **Axes:** +x right, +y up (math-up convention)
- **Units:** abstract, dimensionless. Conventionally generators produce output in roughly `[-1, 1]` on each axis. Renderers handle scaling and y-flip to screen space.

---

## 2. Generator Parameters

All four v1 generators, with parameter ranges and meaning. Ranges are recommended — the kernel does not clamp, but values outside these will produce degenerate or visually uninteresting output.

### 2.1 Modular chord

Parametric circle of `N` points; each point connects to the point at index `round(k · i) mod N`.

```
P_i = (cos(2π i / N), sin(2π i / N)),  i ∈ [0, N)
chord_i: P_i → P_{round(k · i) mod N}
```

| Param | Type | Range | Default | Notes |
|-------|------|-------|---------|-------|
| `N` | int | [3, 10000] | 200 | Point count. Visual sweet spot 100–500. |
| `k` | double | [0.0, 100.0] | 2.0 | Multiplier. Integer values give classic figures (2 = cardioid, 3 = nephroid). Non-integer values morph between them. |

**Special cases worth bundling as presets:**
- `k = 2`: cardioid
- `k = 3`: nephroid
- `k = 4`: 3-cusped epicycloid
- `k = 51, N = 200`: classic "times tables" Mathologer pattern

### 2.2 Hypotrochoid

Spirograph: small circle of radius `r` rolls **inside** big circle of radius `R`, pen offset `d` from rolling centre.

```
x(t) = (R - r) cos(t) + d cos((R - r) / r · t)
y(t) = (R - r) sin(t) - d sin((R - r) / r · t)
```

| Param | Type | Range | Default | Notes |
|-------|------|-------|---------|-------|
| `R` | double | (0, 10] | 5.0 | Outer circle radius |
| `r` | double | (0, R) | 3.0 | Inner circle radius (must be < R for hypotrochoid) |
| `d` | double | [0, 2·R] | 2.0 | Pen offset from rolling centre |
| `samples` | int | [100, 100000] | 4000 | Polyline density |

**Period:** `T = 2π · r / gcd(R, r)` for rational `R/r`. Sample `t ∈ [0, T]` for closed curve. For irrational `R/r` the curve fills an annulus; sample over enough revolutions to feel dense.

**Degenerate cases:** `d = 0` → ellipse (or circle if `R = 2r`). `R = 2r, d = r` → straight line (Tusi couple).

### 2.3 Epitrochoid

Small circle of radius `r` rolls **outside** big circle of radius `R`.

```
x(t) = (R + r) cos(t) - d cos((R + r) / r · t)
y(t) = (R + r) sin(t) - d sin((R + r) / r · t)
```

| Param | Type | Range | Default | Notes |
|-------|------|-------|---------|-------|
| `R` | double | (0, 10] | 3.0 | Stationary circle radius |
| `r` | double | (0, 10] | 1.0 | Rolling circle radius |
| `d` | double | [0, 3·R] | 1.5 | Pen offset |
| `samples` | int | [100, 100000] | 4000 | Polyline density |

**Period:** `T = 2π · r / gcd(R, r)`.

**Degenerate cases:** `R = r, d = r` → cardioid. `R = 2r, d = r` → nephroid.

### 2.4 Lissajous

Two perpendicular sinusoidal oscillators.

```
x(t) = A sin(a · t + φ)
y(t) = B sin(b · t)
```

| Param | Type | Range | Default | Notes |
|-------|------|-------|---------|-------|
| `A` | double | (0, 5] | 1.0 | x-amplitude |
| `B` | double | (0, 5] | 1.0 | y-amplitude |
| `a` | double | [1, 50] | 3.0 | x-frequency |
| `b` | double | [1, 50] | 2.0 | y-frequency |
| `φ` | double | [0, 2π) | π/2 | Phase offset |
| `samples` | int | [100, 100000] | 4000 | Polyline density |

**Closure:** closed curve iff `a / b` is rational. Sample `t ∈ [0, 2π · lcm(a,b) / max(a,b)]` if both are integers. For irrational ratios, the curve precesses indefinitely; sample over enough domain to fill the bounding box.

**Special cases:**
- `a:b = 1:1, φ = π/2`: circle
- `a:b = 1:1, φ = 0`: 45° line
- `a:b = 1:2, φ = π/2`: figure-eight (bowtie)
- `a:b = 3:2, φ = π/2`: trefoil-like

### 2.5 Superformula *(v1.1)*

Gielis's polar-form supershape. One 6-parameter equation captures squares, polygons, stars, flowers, and biological forms.

```
r(φ) = ( |cos(m · φ / 4) / a|^n2 + |sin(m · φ / 4) / b|^n3 )^(-1/n1)
x(φ) = r(φ) cos(φ)
y(φ) = r(φ) sin(φ)
```

| Param | Type | Range | Default | Notes |
|-------|------|-------|---------|-------|
| `m` | double | [0, 20] | 5.0 | Rotational symmetry. Integer → closed at 2π; non-integer fills the disk over multiple periods. |
| `n1` | double | (0, 100] | 1.0 | Primary shape exponent. |
| `n2` | double | [0, 100] | 1.0 | Side exponent. |
| `n3` | double | [0, 100] | 1.0 | Side exponent (asymmetric with `n2` for skewed shapes). |
| `a` | double | (0, 5] | 1.0 | x-scale. |
| `b` | double | (0, 5] | 1.0 | y-scale. |
| `samples` | int | [100, 100000] | 4000 | Polyline density. |

**Special cases:**

- `m = 0, n1 = n2 = n3 = 1`: circle.
- `m = 4, n1 = n2 = n3 = 100`: rounded square.
- `m = 5, n1 = 2, n2 = 7, n3 = 7`: starfish.
- `m = 6, n1 = 1, n2 = 1, n3 = 1`: hexagonal flower.

### 2.6 Phyllotaxis *(v1.1)*

Golden-angle point disk. Consumed as a point scatter or fed to the modular chord rule.

```
P_i = (√i · cos(i · α), √i · sin(i · α)),  i ∈ [0, N)
```

| Param | Type | Range | Default | Notes |
|-------|------|-------|---------|-------|
| `N` | int | [10, 50000] | 1000 | Point count. |
| `alpha` | double | [0, 2π) | 2.39996 (≈ 137.508°) | Divergence angle. Default is the golden angle (sunflower spiral). |
| `mode` | enum | `points` \| `chords` | `points` | `points` renders dots (one per `P_i`); `chords` applies the modular chord rule. |
| `k` | double | [0.0, 100.0] | 2.0 | Modular chord multiplier. Used only when `mode = chords`. |

**Special cases:** `alpha = 137.508°` → canonical sunflower head. Slightly off-golden values (137.0° or 137.6°) give visibly skewed spirals — useful as a parameter to drag.

### 2.7 Strange attractors *(v1.1)*

Iterative 2D orbits. Output is a polyline of `(x_n, y_n)` after burn-in. Three variants:

**Clifford attractor:**

```
x_{n+1} = sin(a · y_n) + c · cos(a · x_n)
y_{n+1} = sin(b · x_n) + d · cos(b · y_n)
```

**de Jong attractor:**

```
x_{n+1} = sin(a · y_n) − cos(b · x_n)
y_{n+1} = sin(c · x_n) − cos(d · y_n)
```

**Tinkerbell map:**

```
x_{n+1} = x_n² − y_n² + a · x_n + b · y_n
y_{n+1} = 2 · x_n · y_n + c · x_n + d · y_n
```

Common parameters (all three):

| Param | Type | Range | Default | Notes |
|-------|------|-------|---------|-------|
| `a` | double | [-3, 3] | varies by attractor | First shape parameter. |
| `b` | double | [-3, 3] | varies | Second shape parameter. |
| `c` | double | [-3, 3] | varies | Third shape parameter. |
| `d` | double | [-3, 3] | varies | Fourth shape parameter. |
| `x0` | double | [-2, 2] | 0.1 | Initial x. |
| `y0` | double | [-2, 2] | 0.1 | Initial y. |
| `burn_in` | int | [0, 100000] | 1000 | Iterations discarded before sampling. |
| `samples` | int | [1000, 1000000] | 100000 | Iterations sampled into the polyline. |

**Canonical parameter sets:**

- Clifford "butterfly": `(a, b, c, d) = (-1.4, 1.6, 1.0, 0.7)`
- de Jong "shell": `(a, b, c, d) = (1.4, -2.3, 2.4, -2.1)`
- Tinkerbell classic: `(a, b, c, d) = (0.9, -0.6013, 2.0, 0.5)`, `(x0, y0) = (-0.72, -0.64)`

**Bounding box:** estimated from a coarse pre-pass (10% of `samples`) since attractors have no analytic extent. If the orbit diverges to infinity or collapses to a fixed point, the renderer surfaces a warning and emits no geometry.

**Determinism caveat:** orbits are deterministic for fixed `(x0, y0)` + parameters within a single toolchain. Cross-architecture byte-identical SVG is not guaranteed (a known caveat of any `double`-precision iterative pipeline; documented in ARCHITECTURE.md §12).

---

## 3. Preset JSON Schema

Authoritative format. Version field is mandatory; readers must reject unknown major versions.

```json
{
  "version": 1,
  "name": "string, free-form",
  "generator": {
    "type": "modular_chord | hypotrochoid | epitrochoid | lissajous | superformula | phyllotaxis | clifford | de_jong | tinkerbell",
    "params": { ... generator-specific, see §2 ... }
  },
  "style": {
    "color_map": {
      "type": "solid | linear_gradient | hsv_sweep | diverging",
      ... type-specific fields ...
    },
    "color_indexer": "by_chord_index | by_chord_length | by_angle | by_curve_t",
    "stroke": {
      "width_min": 0.5,
      "width_max": 0.5,
      "width_indexer": "by_chord_index | by_chord_length | by_angle | by_curve_t",
      "opacity": 0.6
    },
    "background": "#0a0a0a"
  },
  "camera": {
    "centre": [0.0, 0.0],
    "zoom": 1.0
  }
}
```

### 3.1 Color map types

```json
{ "type": "solid", "color": "#rrggbb" }

{ "type": "linear_gradient",
  "start": "#rrggbb",
  "end":   "#rrggbb" }

{ "type": "hsv_sweep",
  "hue_start": 0.0,        // [0, 360)
  "hue_end":   360.0,
  "saturation": 0.8,       // [0, 1]
  "value":      0.95 }

{ "type": "diverging",
  "negative": "#rrggbb",
  "midpoint": "#rrggbb",
  "positive": "#rrggbb" }
```

Colors are 6-digit hex `#rrggbb`. Alpha is carried separately in `stroke.opacity` to keep plotter-mode export clean.

### 3.2 Indexer semantics

The indexer returns a `t ∈ [0, 1]` per primitive, fed to both `color_map` and (independently) the stroke width function.

- `by_chord_index`: `i / (N - 1)` — sequential rainbow.
- `by_chord_length`: `|b - a| / max_length` — normalised by longest chord in the set.
- `by_angle`: `(atan2(b - a) + π) / (2π)` — direction-coloured.
- `by_curve_t`: parameter value along source curve, normalised to `[0, 1]`. Only meaningful for sampled-curve generators.

### 3.3 Validation rules

A reader MUST:

- Reject unknown `version`.
- Reject if `generator.type` is unknown.
- Reject if any generator param is outside its declared range (see §2).
- Default any missing optional field to its declared default.
- Default any missing required-with-default field to its declared default with a warning.

### 3.4 Example: cardioid classic

```json
{
  "version": 1,
  "name": "cardioid-classic",
  "generator": {
    "type": "modular_chord",
    "params": { "N": 200, "k": 2.0 }
  },
  "style": {
    "color_map": {
      "type": "linear_gradient",
      "start": "#1a4480",
      "end":   "#f0c050"
    },
    "color_indexer": "by_chord_length",
    "stroke": {
      "width_min": 0.5,
      "width_max": 0.5,
      "width_indexer": "by_chord_index",
      "opacity": 0.6
    },
    "background": "#0a0a0a"
  },
  "camera": {
    "centre": [0.0, 0.0],
    "zoom": 1.0
  }
}
```

---

## 4. SVG Output Conventions

### 4.1 Document structure

```xml
<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg"
     viewBox="x y width height"
     width="..." height="...">
  <rect x="..." y="..." width="..." height="..."
        fill="#background"/>
  <g id="layer-0" inkscape:label="modular_chord">
    <line x1="..." y1="..." x2="..." y2="..."
          stroke="#rrggbb" stroke-width="0.5"
          stroke-opacity="0.6"/>
    ...
  </g>
</svg>
```

- `viewBox` is sized to the geometry bounding box plus configurable margin (default: 5%).
- One `<g>` per logical layer (one per generator instance — multi-layer scenes in Phase 9 produce multiple groups).
- Each group is labelled with the Inkscape namespace for editor friendliness.
- Stroke colour as `#rrggbb`. Opacity carried as `stroke-opacity` attribute, never embedded in the colour, so plotter mode can strip it cleanly.
- Background is a single `<rect>` covering the viewBox, drawn first.
- Coordinates use up-to-six decimal places (sufficient for any practical print size; avoids file bloat).

### 4.2 Plotter mode

When `--plotter` is set on the CLI or the Plotter mode toggle is enabled in the GUI:

- Background `<rect>` omitted.
- All strokes use the same colour (default `#000000`).
- `stroke-opacity` omitted entirely.
- Lines sorted by start point using a nearest-neighbour heuristic to minimise pen travel.
- No layer groups (single flat `<g>`).

### 4.3 Determinism

Same preset → byte-identical SVG. Achieved by:

- Deterministic generator iteration order
- Fixed decimal precision in coordinate formatting
- No timestamps in output
- No `<!-- comments -->` containing variable data

A Phase 7 CI test asserts byte-for-byte equality across consecutive runs.

---

## 5. CLI Interface

```
caustic-cli [OPTIONS] PRESET_FILE -o OUTPUT.svg
```

### Required arguments

| Arg | Meaning |
|-----|---------|
| `PRESET_FILE` | Path to a preset JSON file |
| `-o, --output FILE` | Output SVG path |

### Options

| Option | Default | Meaning |
|--------|---------|---------|
| `--width FLOAT` | 1024 | Output viewBox width (abstract units) |
| `--height FLOAT` | 1024 | Output viewBox height |
| `--margin FLOAT` | 0.05 | Margin around geometry as fraction of bounding box |
| `--plotter` | off | Enable plotter mode (§4.2) |
| `--simplify EPS` | off | Apply Douglas–Peucker simplification at tolerance ε |
| `--seed UINT` | 0 | RNG seed (reserved for future generators that need it) |
| `-h, --help` | — | Print usage and exit |
| `-V, --version` | — | Print version and exit |

### Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success |
| 1 | Generic error |
| 2 | Preset file not found or unreadable |
| 3 | Preset validation failed (invalid JSON or schema mismatch) |
| 4 | Output write failed |

---

## 6. Versioning Policy

- Preset JSON `version` field follows independent integer versioning. Bumped on schema changes.
- A reader must support reading its own version and at least the previous major version.
- Caustic's binary version follows SemVer once a public 1.0 release exists.

---

## 7. Reserved for future versions

These fields appear in the schema in v1.1+ but should be ignored by v1 readers:

- `generator.layers[]` — multi-layer scenes (Phase 9)
- `animation` — parameter envelopes (Phase 8)
- `metadata.author`, `metadata.created` — preset attribution
