# CLAUDE.md patch — DiamondStack generator

> **Status:** Active
> **Provenance:** Claude (primary author)
> **Last reviewed:** 2026-05-11
> **Why this status:** DiamondStack generator designed and ready for integration. Files produced, wiring needed.

---

## Task

Integrate the new `DiamondStack` generator into Caustic. All source files have been produced; this document lists every edit needed to wire them together. Follow the sections in order; each section names the target file and shows the exact str_replace to apply.

---

## 1. Copy new files into the repo

```bash
# From wherever the generated files landed:
cp diamond_stack.hpp          core/include/caustic/generators/diamond_stack.hpp
cp diamond_stack_classic.json presets/
cp diamond_stack_two_colour.json presets/
cp diamond_star_neon.json     presets/
cp square_grid_2x3.json       presets/
```

---

## 2. `core/include/caustic/preset.hpp`

### 2a — Add `DiamondStackParams` struct (after `TinkerbellParams`, before `GeneratorSpec`)

Find the block ending with `};` for `TinkerbellParams` and insert immediately after it:

```cpp
struct DiamondStackParams {
    int    n_modules    = 3;    // stacked hourglass modules (1–20)
    int    N            = 80;   // strings per fan (2–500)
    double aspect       = 0.6;  // half_waist_width / half_module_height
    double rotation_rad = 0.0;
};
```

### 2b — Add enum value to `GeneratorType`

In the `enum class GeneratorType` block, after `Tinkerbell,` add:
```cpp
    DiamondStack,
```

### 2c — Add member to `GeneratorSpec`

In `struct GeneratorSpec`, after `TinkerbellParams tink;` add:
```cpp
    DiamondStackParams dstack;
```

---

## 3. `core/include/caustic/geometry_factory.hpp`

### 3a — Add include at top of the block

After `#include <caustic/generators/attractors.hpp>` add:
```cpp
#include <caustic/generators/diamond_stack.hpp>
```

### 3b — Add case in `geometry_from_spec`

In the `switch (g.type)` block, after the `Tinkerbell` case's closing `break;`, add:

```cpp
        case GeneratorType::DiamondStack: {
            const int n = coarse ? std::max(2, g.dstack.N / 4) : g.dstack.N;
            geo.chords = diamond_stack(g.dstack.n_modules, n,
                                       g.dstack.aspect, g.dstack.rotation_rad);
            break;
        }
```

---

## 4. `core/include/caustic/preset_io.hpp`

### 4a — `generator_type_to_string`

After `case GeneratorType::Tinkerbell: return "tinkerbell";` add:
```cpp
        case GeneratorType::DiamondStack: return "diamond_stack";
```

### 4b — `generator_type_from_string`

After `if (s == "tinkerbell") return GeneratorType::Tinkerbell;` add:
```cpp
    if (s == "diamond_stack") return GeneratorType::DiamondStack;
```

### 4c — `generator_to_json` (the `switch` in that function)

After the `Tinkerbell` case block, add:
```cpp
        case GeneratorType::DiamondStack:
            gp["n_modules"]    = gs.dstack.n_modules;
            gp["N"]            = gs.dstack.N;
            gp["aspect"]       = gs.dstack.aspect;
            gp["rotation_rad"] = gs.dstack.rotation_rad;
            break;
```

### 4d — `generator_from_json` (the `switch` in that function)

After the `Tinkerbell` case block, add:
```cpp
        case GeneratorType::DiamondStack:
            gs.dstack.n_modules    = gp.value("n_modules",    gs.dstack.n_modules);
            gs.dstack.N            = gp.value("N",            gs.dstack.N);
            gs.dstack.aspect       = gp.value("aspect",       gs.dstack.aspect);
            gs.dstack.rotation_rad = gp.value("rotation_rad", gs.dstack.rotation_rad);
            break;
```

---

## 5. `app/main.cpp`

### 5a — `kGeneratorNames` array

After `"tinkerbell",` add:
```cpp
    "diamond stack",
```

### 5b — Parameter panel `switch` block

After the `Tinkerbell` case closing brace, add:

```cpp
        case caustic::GeneratorType::DiamondStack:
            if (slider_int_w("modules", &p.generator.dstack.n_modules, 1, 20)) state.dirty = true;
            if (slider_int_w("N",       &p.generator.dstack.N,         2, 500)) state.dirty = true;
            if (slider_double_w("aspect",
                                &p.generator.dstack.aspect, 0.1, 3.0, 0.01, "%.3f")) state.dirty = true;
            if (slider_double_w("rotation",
                                &p.generator.dstack.rotation_rad,
                                -std::numbers::pi, std::numbers::pi, 0.01)) state.dirty = true;
            ImGui::TextDisabled("Tip: duplicate this layer, change aspect + colour → two-tone hourglass");
            ImGui::TextDisabled("Tip: Layers panel → Apply rotational array (N=4) → neon star");
            break;
```

### 5c — Reset generator params block

After `case caustic::GeneratorType::Tinkerbell: p.generator.tink = caustic::TinkerbellParams{}; break;` add:
```cpp
            case caustic::GeneratorType::DiamondStack: p.generator.dstack = caustic::DiamondStackParams{}; break;
```

### 5d — Keys 1–4 shortcut block (optional, no change needed)

Keys 1–4 stay wired to the first four generators. DiamondStack is reachable via the Generator dropdown only. No change needed here.

---

## 6. `tests/CMakeLists.txt`

In the `add_executable(test_caustic …)` source list, add:
```cmake
    test_diamond_stack.cpp
```

In the `foreach(preset …)` CLI smoke list, add:
```cmake
                   diamond_stack_classic diamond_stack_two_colour
                   diamond_star_neon square_grid_2x3
```

---

## 7. `tests/test_diamond_stack.cpp` (create new file)

```cpp
#include <doctest/doctest.h>

#include <cmath>

#include <caustic/generators/diamond_stack.hpp>

using namespace caustic;

TEST_CASE("diamond_stack returns 2*n_modules*N chords") {
    const auto cs = diamond_stack(3, 60, 0.6);
    CHECK(cs.size() == static_cast<std::size_t>(2 * 3 * 60));
}

TEST_CASE("diamond_stack n_modules=1 gives 2*N chords") {
    CHECK(diamond_stack(1, 40, 0.6).size() == 80);
}

TEST_CASE("diamond_stack with N<2 returns empty") {
    CHECK(diamond_stack(3, 1, 0.6).empty());
    CHECK(diamond_stack(3, 0, 0.6).empty());
}

TEST_CASE("diamond_stack with n_modules<=0 returns empty") {
    CHECK(diamond_stack(0, 60, 0.6).empty());
    CHECK(diamond_stack(-1, 60, 0.6).empty());
}

TEST_CASE("t_along is normalised to [0, 1)") {
    const auto cs = diamond_stack(3, 60, 0.6);
    for (const auto& c : cs) {
        CHECK(c.t_along >= 0.0);
        CHECK(c.t_along <= 1.0);
    }
}

TEST_CASE("t_along increases monotonically") {
    const auto cs = diamond_stack(3, 60, 0.6);
    for (std::size_t i = 1; i < cs.size(); ++i) {
        CHECK(cs[i].t_along >= cs[i - 1].t_along);
    }
}

TEST_CASE("rotation_rad=0 produces vertical stack centred at origin") {
    // With no rotation, all waist (L/R) points should have x != 0,
    // and all tip (T/B) points should have x == 0.
    // The first chord of each upper fan connects tip T to a waist point:
    // a = lerp(T, L, 0) = T = (0, y_top), b = lerp(T, R, 1) = R = (+hw, y_mid).
    const auto cs = diamond_stack(1, 10, 0.6);
    // First chord: a.x should be ~0 (at tip), b.x should be +hw
    constexpr double eps = 1e-9;
    CHECK(std::abs(cs[0].a.x) < eps);   // tip point on axis
    CHECK(cs[0].b.x > 0.0);             // right waist
}

TEST_CASE("rotation_rad=pi/2 rotates stack to horizontal") {
    const auto vert = diamond_stack(1, 10, 0.6, 0.0);
    const auto horiz = diamond_stack(1, 10, 0.6, std::numbers::pi / 2.0);
    // First chord's 'a' point should swap x and y (approx) after 90° rotation.
    constexpr double eps = 1e-6;
    CHECK(std::abs(horiz[0].a.y) < eps);   // what was on y-axis is now on x-axis
}

TEST_CASE("stack bounding box is approximately [-hw, -0.5] to [+hw, +0.5]") {
    const int nm = 3;
    const double asp = 0.6;
    const auto cs = diamond_stack(nm, 60, asp);
    const double mh = 1.0 / nm;
    const double hw = asp * mh * 0.5;
    for (const auto& c : cs) {
        for (const Vec2& v : {c.a, c.b}) {
            CHECK(v.x >= -hw - 1e-9);
            CHECK(v.x <= +hw + 1e-9);
            CHECK(v.y >= -0.5 - 1e-9);
            CHECK(v.y <= +0.5 + 1e-9);
        }
    }
}

TEST_CASE("preset round-trip: diamond_stack params survive JSON") {
    // Build a minimal preset, serialise, deserialise, check params survive.
    caustic::Preset p;
    auto& l = p.scene.layers[0];
    l.generator.type = caustic::GeneratorType::DiamondStack;
    l.generator.dstack.n_modules = 5;
    l.generator.dstack.N = 120;
    l.generator.dstack.aspect = 1.2;
    l.generator.dstack.rotation_rad = 0.785;

    nlohmann::json j;
    caustic::to_json(j, p);
    caustic::Preset p2;
    caustic::from_json(j, p2);

    const auto& d = p2.scene.layers[0].generator.dstack;
    CHECK(d.n_modules == 5);
    CHECK(d.N == 120);
    CHECK(std::abs(d.aspect - 1.2) < 1e-9);
    CHECK(std::abs(d.rotation_rad - 0.785) < 1e-9);
}
```

(Add `#include <caustic/preset_io.hpp>` and `#include <nlohmann/json.hpp>` at the top for the round-trip test.)

---

## 8. `SPEC.md` — add §2.8

Insert after the Tinkerbell section (§2.7) and before §3:

```markdown
### 2.8 Diamond stack *(v1.1)*

Stacked hourglass / diamond string art. Each module has a top tip T, bottom tip B,
and two waist points L (left) and R (right). Two parabolic fans per module are generated
by connecting points along T→L to points along T→R (reversed index), and B→L to B→R.
Adjacent modules share their tip points, producing the pinched-waist hourglass silhouette.

```
Upper fan:  A[i] = lerp(T, L, i/(N-1))  →  B[i] = lerp(T, R, 1 - i/(N-1))
Lower fan:  A[i] = lerp(B, L, i/(N-1))  →  B[i] = lerp(B, R, 1 - i/(N-1))
```

The stack is centred at the origin and normalised to total height 1.0.

| Param | Type | Range | Default | Notes |
|-------|------|-------|---------|-------|
| `n_modules` | int | [1, 20] | 3 | Number of stacked hourglass modules. |
| `N` | int | [2, 500] | 80 | Strings per fan. Visual sweet spot 40–120. |
| `aspect` | double | [0.1, 3.0] | 0.6 | half_waist_width / half_module_height. <1 = tall/thin, >1 = wide/flat. |
| `rotation_rad` | double | [−π, π] | 0.0 | Rotate entire stack. Use with multi-layer scenes for radial star patterns. |

**Multi-layer recipes:**

*Two-colour hourglass (Image 1 style):*
Two DiamondStack layers with different `aspect` values and different solid colours.
Layer 0: `aspect=0.60`, white, `opacity=0.65`. Layer 1: `aspect=0.35`, red, `opacity=0.80`.

*Neon radial star (Image 2 style):*
Three or four DiamondStack layers at `rotation_rad = 0, π/2, π/4, 3π/4` (or use
Layers panel → Apply rotational array with N=4 on a single stack layer).

*Instant workflow:* Load `diamond_stack_classic`, duplicate the layer, set different
`aspect` and colour on the copy — the two-colour hourglass takes under a minute.
```

---

## 9. `CLAUDE.md` active task update

Replace the "Next:" line with:

```
Next: **Phase 12 — Polish & release**, plus Phase 9 Stage B (rose, Maurer rose,
superformula curve, phyllotaxis, hybrid mode — the only Stage B items not yet landed).

**DiamondStack just added (2026-05-11):** New `diamond_stack` generator producing
stacked hourglass / parabolic-fan patterns in a single layer. Parameters: `n_modules`,
`N`, `aspect`, `rotation_rad`. Two-colour hourglass (Image 1 style) = two layers with
different `aspect`. Neon star (Image 2 style) = rotational_array N=4 on one stack.
Bundled presets: `diamond_stack_classic`, `diamond_stack_two_colour`,
`diamond_star_neon`, `square_grid_2x3`. Test file: `test_diamond_stack.cpp`.
```

---

## 10. `CHANGELOG.md` entry

Add at top of `## [Unreleased]`:

```markdown
- DiamondStack generator: `diamond_stack(n_modules, N, aspect, rotation_rad)` in
  `core/include/caustic/generators/diamond_stack.hpp`. Two parabolic fans per module
  (upper fan: lines connect T→L arm to T→R arm reversed; lower fan: B→L to B→R
  reversed). Adjacent modules share their tip points producing the pinched hourglass
  waist. `n_modules` / `N` / `aspect` / `rotation_rad` round-trip through JSON
  preset schema. Coarse mode: N÷4. Four bundled presets ship: `diamond_stack_classic`
  (single layer, HSV), `diamond_stack_two_colour` (white outer + red inner, matches
  reference Image 1), `diamond_star_neon` (four stacks at 0°/45°/90°/135°, matches
  Image 2), `square_grid_2x3` (polygon_chord on 4-gon × 6 grid tiles, matches
  Image 4). Test file `test_diamond_stack.cpp` adds 9 cases (chord count, edge cases,
  t_along normalisation + monotonicity, rotation, bounding box, JSON round-trip).
```

---

## 11. `ROADMAP.md` — add to Phase 12 deliverables

Under Phase 12 `[ ]` items add:
```markdown
- [ ] DiamondStack generator shipped in bundled preset gallery
- [ ] README section "Making your first hourglass" using `diamond_stack_two_colour` as the worked example
```

---

## Verification checklist (run after all edits)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
# Expect: all previous tests pass + 9 new diamond_stack tests
# CLI smoke: 4 new preset renders pass

./build/app/caustic
# UI check:
#   Generator dropdown shows "diamond stack"
#   Load presets/diamond_stack_two_colour.json — two-colour hourglass visible
#   Load presets/diamond_star_neon.json — 4-colour star visible
#   Duplicate a DiamondStack layer, change aspect slider — instant two-tone
#   Apply rotational array N=6 on a single DiamondStack — hexagonal star
```

---

## Pitfalls

- **Reversal is load-bearing.** The parabolic envelope emerges from connecting
  `arm1[i]` to `arm2[N-1-i]`. Removing the reversal (connecting same index) produces
  a filled triangle, not a curved envelope. Do not change this.
- **`aspect` range.** Values above ~2.5 cause the waist to extend past the adjacent
  module's tip, which produces visually correct but geometrically overlapping geometry.
  The UI slider is clamped to [0.1, 3.0]; the kernel accepts any value.
- **Coarse mode.** N must remain ≥ 2 even in coarse mode (`std::max(2, N/4)`).
  N=1 returns empty (the existing guard).
- **`t_along` assignment.** Assigned as `i / total_chords` across all chords in
  emission order (UF then LF, module 0 then module n-1). This means
  `by_chord_index` color sweeps top-to-bottom. `by_chord_length` gives the most
  visually interesting result for multi-colour presets (short chords near tips,
  long chords near the X-crossing waist).
