# Build Instructions

Everything needed to take a fresh checkout of Caustic and produce working executables.

## Supported platforms

| Platform | Compiler | Notes |
|----------|----------|-------|
| Linux x86_64 | GCC 11+, Clang 14+ | Primary development target (Ubuntu 22.04 LTS or newer) |
| Windows x86_64 | mingw-w64 (cross-compile) or MSVC 19.30+ | Cross-compile from Linux is the supported path |
| macOS | not officially supported | May work via Clang; not regularly tested |

## Prerequisites

### Linux (Ubuntu / Debian)

```bash
sudo apt update
sudo apt install -y build-essential cmake git ninja-build \
  libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev \
  libxcursor-dev libxi-dev libasound2-dev pkg-config
```

The X11 / GL development packages are raylib's transitive requirements.

### Linux (Fedora / RHEL)

```bash
sudo dnf install -y gcc-c++ cmake git ninja-build \
  mesa-libGL-devel libX11-devel libXrandr-devel libXinerama-devel \
  libXcursor-devel libXi-devel alsa-lib-devel pkgconf
```

### Windows (cross-compile from Linux)

```bash
sudo apt install -y mingw-w64
```

Toolchain file at `cmake/toolchains/mingw-w64-x86_64.cmake` (added in Phase 0).

## Dependencies

All third-party libraries are pulled via CMake `FetchContent` at configure time. No manual installation required.

| Dependency | Pinned version | Purpose |
|-----------|----------------|---------|
| raylib | 6.0 | Window, input, rendering |
| rlImGui | `Raylib_6_0` tag (tracks the matching raylib release) | Dear ImGui binding for raylib |
| Dear ImGui | v1.92.7 | Immediate-mode UI (compiled as our own target — upstream ships no CMakeLists) |
| nlohmann/json | v3.11.3 | Preset serialisation |
| doctest | v2.4.11 | Unit tests |

rlImGui's `Raylib_6_0` tag uses ImGui v1.92's texture-manager APIs (`GetPlatformIO`, `ImTextureData`, `ImGuiBackendFlags_RendererHasTextures`); ImGui must be ≥ 1.92 for that tag to compile. rlImGui and raylib must be bumped in lockstep — see `Raylib_*` tags upstream.

## Build

### Debug build with tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug -G Ninja
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Release build

```bash
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -G Ninja
cmake --build build-release -j
```

### Run

```bash
./build-release/app/caustic                    # GUI
./build-release/cli/caustic-cli --help         # headless
./build-release/cli/caustic-cli presets/cardioid_classic.json -o out.svg
```

## Cross-compilation: Windows from Linux

```bash
cmake -B build-win64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/mingw-w64-x86_64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -G Ninja
cmake --build build-win64 -j
```

Output executables: `build-win64/app/caustic.exe`, `build-win64/cli/caustic-cli.exe`. Bundle with required runtime DLLs (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll`) for itch.io distribution.

## Build options

| Option | Default | Effect |
|--------|---------|--------|
| `CAUSTIC_BUILD_TESTS` | `ON` | Build the doctest harness |
| `CAUSTIC_BUILD_APP` | `ON` | Build the GUI executable |
| `CAUSTIC_BUILD_CLI` | `ON` | Build the headless CLI |
| `CAUSTIC_USE_SYSTEM_RAYLIB` | `OFF` | Use system-installed raylib instead of FetchContent |

For a CLI-only headless build (e.g. inside a Docker container with no display server):

```bash
cmake -B build-cli \
  -DCAUSTIC_BUILD_APP=OFF \
  -DCMAKE_BUILD_TYPE=Release
cmake --build build-cli -j
```

This skips raylib's window/GL dependencies entirely.

## Troubleshooting

### `error: GL/gl.h: No such file or directory`

Missing OpenGL development headers. Install `libgl1-mesa-dev` (Debian) or `mesa-libGL-devel` (Fedora).

### `error: X11/Xlib.h: No such file or directory`

Missing X11 development headers. See the prerequisites section above.

### CMake configures but rlImGui fails to build

rlImGui is pinned to specific raylib versions. If you've manually overridden the raylib version, it may now be incompatible. Either revert to the pinned versions or update both together — see rlImGui's README for its compatibility matrix.

### CLI build pulls in raylib unexpectedly

Architectural invariant violated — `cli/` should link only `core/` and `render/svg_renderer`. Audit the CMake `target_link_libraries` chain. The CLI must build with `CAUSTIC_BUILD_APP=OFF` and no raylib.

### Window opens but renders garbage on integrated GPU

Some integrated drivers handle `GL_LINE_SMOOTH` poorly. Set `CAUSTIC_DISABLE_LINE_SMOOTH=ON` at configure time, or upgrade Mesa.

### "DLL not found" on Windows

The mingw runtime DLLs aren't bundled by default. Copy `libgcc_s_seh-1.dll`, `libstdc++-6.dll`, `libwinpthread-1.dll` from `/usr/x86_64-w64-mingw32/lib/` (path may vary) next to the `.exe`.

## CI

Phase 0 will add a minimal GitHub Actions workflow:

- Linux GCC + Clang debug builds with tests
- Linux release build
- Windows cross-compile

Workflow file lives at `.github/workflows/ci.yml` once added.
