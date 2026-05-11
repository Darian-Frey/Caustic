> **Status:** Active — v1.1 feature-complete, polish & release pending
> **Provenance:** Shane Hartley (Darian-Frey, primary author); Claude (architect, implementation assistant, doc scaffolding)
> **Last reviewed:** 2026-05-11
> **Why this status:** Phases 0–11 closed. 12 generators across three pipeline tiers; multi-layer scenes; animation system; SVG export; headless CLI. Only Phase 12 (README screenshots, expanded preset gallery, license, Windows cross-compile, itch.io page) remains for the public 1.0.

# Caustic

A C++20 desktop studio for generative geometric art — chord patterns ("string art"), roulette curves ("spirograph"), parametric envelopes, and strange attractors. Live parameter editing via rlImGui, multi-layer scenes with array tools, parameter-envelope animation with frame export, and deterministic vector-perfect SVG output.

The name comes from optics: a *caustic* is the envelope curve formed where many rays converge after reflection or refraction off a curved surface. That's precisely what string art evokes — the smooth shape that emerges from a dense fan of straight lines is, mathematically, a caustic.

## Quick start

```bash
git clone https://github.com/Darian-Frey/Caustic.git
cd Caustic
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/app/caustic                                         # launch GUI
./build/cli/caustic-cli presets/cardioid_classic.json -o out.svg
./build/cli/caustic-cli --help                              # headless flags
```

## Generators

Twelve generators across three pipeline tiers. All produce math-up world coordinates that the renderers fit to canvas.

**Chord sets** — pairs of points joined by straight lines:

- **Modular chord** — `i → (k·i mod N)` on N points around a circle. Cardioid at k=2, nephroid at k=3, times-table cusps at k=51.
- **Polygon chord** — same modular rule on N points along a regular n-gon's perimeter. Deltoid envelope at n=3, k=2; hexagram at n=6, k=2.
- **Phyllotaxis (chord)** — modular rule on the golden-angle disk. Sunflower spirals overlaid with chord patterns.
- **Linear envelope** — chord set between two line segments. Perpendicular + k=1 = parabolic corner fan; parallel + k=−1 = bowtie.

**Parametric curves** — closed-form `t → (x, y)` sampled to a polyline:

- **Hypotrochoid / Epitrochoid** — classical roulettes (Spirograph patterns)
- **Lissajous** — `(A·sin(a·t + φ), B·sin(b·t))`
- **Rose (rhodonea)** — `r = cos(n·θ/d)`, n and d integer
- **Superformula** — Gielis equation; polygons, stars, organic forms via 6 parameters

**Iterative orbits** — `(x_n, y_n) → (x_{n+1}, y_{n+1})` traced as polyline:

- **Clifford** — `(sin(a·y) + c·cos(a·x), sin(b·x) + d·cos(b·y))`
- **de Jong** — `(sin(a·y) − cos(b·x), sin(c·x) − cos(d·y))`
- **Tinkerbell** — `(x² − y² + a·x + b·y, 2·x·y + c·x + d·y)`; divergence-protected with truncation

## Features

- **Multi-layer scenes** with per-layer transform (translate / rotate / scale / mirror) and visibility toggle.
- **Array tools** — rotational, grid, mirror-reflect — produce N concrete layers from a seed layer with one click.
- **Style system** — solid / linear-gradient / HSV-sweep / diverging colour maps, four indexers (chord index, length, angle, curve t), variable stroke width, opacity, cyclic-curve continuity flag.
- **Camera** — middle-drag pan, scroll-zoom, F or 0 reset, F11 fullscreen.
- **Preset save/load** to XDG dir (`$XDG_CONFIG_HOME/caustic/presets/`). 17 bundled presets covering all generator families.
- **SVG export** with plotter mode (single colour, no opacity, sorted chords as one pen-down per curve).
- **Animation system** — `Static` / `Linear` / `Sine` envelopes driving 28 animatable parameters (generator coefficients, layer transforms, camera zoom); bake to numbered SVG sequence.
- **Headless CLI** — `caustic-cli preset.json -o out.svg` for batch/scripted use. Exit codes match `SPEC.md §5`.

## Build requirements

- C++20 compiler (GCC 11+, Clang 14+, MSVC 19.30+)
- CMake 3.20+
- Linux primary; Windows via mingw-w64 cross-compile (Phase 12)

Dependencies are pulled in via CMake `FetchContent`: raylib 6.0, rlImGui (`Raylib_6_0` tag), Dear ImGui v1.92.7, nlohmann/json v3.11.3, doctest v2.4.11. No system packages required beyond the toolchain. See [BUILD.md](BUILD.md) for full setup, cross-compilation, and troubleshooting.

## Project structure

```
caustic/
├── core/         math kernel — zero raylib dependency, double precision throughout
├── render/       split into caustic-render-svg (always built) and caustic-render-raylib
├── app/          GUI executable (rlImGui)
├── cli/          headless executable
├── tests/        doctest harness — 123 cases + 23 CTest CLI smoke tests
└── presets/      17 bundled JSON presets
```

## Documentation

- [MANUAL.md](MANUAL.md) — user manual: every control, every generator, common recipes
- [ARCHITECTURE.md](ARCHITECTURE.md) — design decisions, module boundaries, rendering model
- [SPEC.md](SPEC.md) — preset JSON schema, generator parameters, SVG conventions, CLI interface
- [ROADMAP.md](ROADMAP.md) — phased build plan; phases 0–11 closed, Phase 12 remaining
- [BUILD.md](BUILD.md) — environment, toolchain, dependencies, troubleshooting
- [CHANGELOG.md](CHANGELOG.md) — version history
- [BUGS.md](BUGS.md) — debugging journal: symptom-first log of bugs and their fixes
- [CLAUDE.md](CLAUDE.md) — handoff document for AI-assisted development sessions

## License

**TBD** — license decision deferred to Phase 12. Until a `LICENSE` file lands, this repo is under default "all rights reserved." Likely candidates: MIT (permissive, fits the itch.io distribution model) or GPL-3.0.
