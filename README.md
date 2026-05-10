> **Status:** Active
> **Provenance:** Shane Hartley (Darian-Frey, primary author); Claude (architect, doc scaffolding)
> **Last reviewed:** 2026-05-10
> **Why this status:** Repo just created. Phase 0 setup imminent.

# Caustic

A C++20 desktop studio for generative geometric art — chord patterns ("string art") and roulette curves ("spirograph") with live parameter editing and vector-perfect SVG export.

The name comes from optics: a *caustic* is the envelope curve formed where many rays converge after reflection or refraction off a curved surface. That's precisely what string art evokes — the smooth shape that emerges from a dense fan of straight lines is, mathematically, a caustic.

## Quick start

Phase 0 not yet implemented. Once bootstrapped:

```bash
git clone https://github.com/Darian-Frey/Caustic.git
cd Caustic
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/app/caustic              # launch GUI
./build/cli/caustic-cli --help   # headless mode
```

## Build requirements

- C++20 compiler (GCC 11+, Clang 14+, MSVC 19.30+)
- CMake 3.20+
- Linux primary; Windows via mingw-w64 cross-compile

Dependencies are pulled in via CMake `FetchContent`: raylib, rlImGui, nlohmann/json, doctest. No system packages required beyond the toolchain.

See [BUILD.md](BUILD.md) for full setup, cross-compilation, and troubleshooting.

## Project structure

```
caustic/
├── core/         math kernel, no raylib dependency
├── render/       raylib + SVG renderers
├── app/          GUI executable (rlImGui)
├── cli/          headless executable
├── tests/        doctest harness
└── presets/      bundled JSON presets
```

## Documentation

- [ARCHITECTURE.md](ARCHITECTURE.md) — design decisions, module boundaries, rendering model
- [SPEC.md](SPEC.md) — preset JSON schema, generator parameters, SVG conventions, CLI interface
- [ROADMAP.md](ROADMAP.md) — phased build plan with acceptance criteria
- [BUILD.md](BUILD.md) — environment, toolchain, dependencies, troubleshooting
- [CHANGELOG.md](CHANGELOG.md) — version history
- [CLAUDE.md](CLAUDE.md) — handoff document for AI-assisted development sessions

## License

**TBD** — license selection deferred to Phase 10 per architecture §11. Until a LICENSE file lands, this repo is under default "all rights reserved." Likely candidates: MIT (permissive, fits the itch.io distribution model) or GPL-3.0 (matches ARCHIVIST and similar tools).
