# AppImage packaging

Recipe for producing `Caustic-x86_64.AppImage` — a single-file portable Linux binary that bundles the GUI, the bundled presets, and every non-system shared library Caustic links against.

## Prerequisites

Two single-file binaries from the AppImage ecosystem:

```bash
# linuxdeploy — walks ldd on the binary and bundles non-system libs into AppDir/usr/lib
wget https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage -O linuxdeploy
chmod +x linuxdeploy

# appimagetool — packs an AppDir into an AppImage
wget https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage -O appimagetool
chmod +x appimagetool

# Put both on PATH (e.g. ~/.local/bin/ or wherever you keep single-file tools).
sudo mv linuxdeploy appimagetool /usr/local/bin/
```

You also need the usual Caustic build prereqs: a C++20 toolchain (GCC 11+ / Clang 14+), CMake 3.20+, and your distro's X11/GL/Wayland dev headers. See [`BUILD.md`](../../BUILD.md) for the full list.

## Build

From the repo root:

```bash
./packaging/appimage/build-appimage.sh
```

The script does four things:

1. **Configure + build** a Release `caustic` binary into `build-appimage/`.
2. **Lay out** an `AppDir/` at the repo root with the binary at `usr/bin/`, every bundled preset at `usr/share/caustic/presets/`, the icon at `usr/share/icons/hicolor/scalable/apps/`, the `.desktop` file at `usr/share/applications/`, and an `AppRun` shim at the top.
3. **Bundle shared libraries** via `linuxdeploy` — copies non-system `.so` files into `usr/lib/` and patches the binary's RPATH so they're found first.
4. **Pack** the AppDir into `Caustic-x86_64.AppImage` via `appimagetool`.

Expect a ~15–25 MB output file. The first build takes a few minutes (raylib + ImGui + dependencies); subsequent rebuilds are incremental.

## Run

```bash
chmod +x Caustic-x86_64.AppImage
./Caustic-x86_64.AppImage
```

The AppImage is fully portable — copy it to any modern x86_64 Linux box and double-click. No install step, no root, no system packages.

## How the bundled-preset path works

When you launch from an AppImage, the cwd is wherever the user double-clicked — usually their home directory or `~/Downloads`. The app's `find_bundled_preset_dir()` (in [`app/main.cpp`](../../app/main.cpp)) tries, in order:

1. `./presets` (the cwd) — repo-root dev workflow
2. `<exe>/../share/caustic/presets` — what the AppImage installs
3. `<exe>/presets` — portable side-by-side layout

The AppImage path is the second one. User presets continue to come from `$XDG_CONFIG_HOME/caustic/presets/` (the existing XDG dir), which is writable and persists across AppImage versions.

## Customising the icon

[`caustic.svg`](caustic.svg) is a 256×256 SVG of a 24-chord modular-chord cardioid (the canonical Caustic demo). Replace it with any 256×256 PNG or SVG and re-run the script.

## Troubleshooting

**`linuxdeploy not found`** — install it (see Prerequisites). The script checks `$PATH` before doing anything.

**`AppImage runs but bundled presets are missing`** — the user is on a build *not* produced by this script (e.g. raw `./build/app/caustic`). The fix here is the path-lookup change in [`app/main.cpp`](../../app/main.cpp) — make sure your binary is built from a tree containing that change.

**`AppImage runs but ImGui windows are blank / GL errors on startup`** — older `linuxdeploy` builds sometimes pull in conflicting Mesa libs. Try the `continuous` tag (see Prerequisites) rather than a pinned release.

**`AppImage runs but the mp4 bake silently produces no file`** — mp4 export shells out to `ffmpeg` on the host's `$PATH`. The AppImage doesn't bundle it (would balloon the size into the hundreds of MB). Install `ffmpeg` system-wide for that feature; PNG sequence and GIF bakes work without it.
