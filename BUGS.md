# Bugs

Chronological log of bugs encountered during Caustic's development and how they were resolved. Newest first. Each entry: **symptom → root cause → fix → files**.

The goal is to keep this file useful when something breaks unexpectedly — search for the symptom first. If you fix a bug, leave a note here so the next person (or AI session) doesn't relearn it.

---

## 2026-05-18 — JPEG export writes nothing, raylib logs "Export image format requested not supported"

**Symptom:** After the Export panel learned about PNG and JPEG via `RaylibRenderer::write_image` (which calls raylib's `ExportImage`, format inferred from the path's extension), PNG worked fine but JPEG silently failed. Console showed `WARNING: IMAGE: Export image format requested not supported` and `WARNING: FILEIO: [.../test.jpg] Failed to export image` for every JPEG attempt.

**Root cause:** raylib's `ExportImage` is **gated by build-time flags** — it only handles the formats whose `SUPPORT_FILEFORMAT_*` macro is defined when `rtextures.c` is compiled. raylib 6.0's `config.h` enables **only PNG** by default; every other format (JPG, BMP, TGA, GIF, QOI, …) ships with its define **commented out**:

```c
#define SUPPORT_FILEFORMAT_PNG      1
//#define SUPPORT_FILEFORMAT_JPG    1
//#define SUPPORT_FILEFORMAT_BMP    1
```

`ExportImage` dispatches on extension and falls through to the warning when the requested format wasn't compiled in. Our code path looked correct; the library just couldn't honour it.

**Fix:** Pre-define the macros on the raylib target before its sources are compiled. In `CMakeLists.txt`, right after `FetchContent_MakeAvailable(raylib)`:

```cmake
target_compile_definitions(raylib PRIVATE
    SUPPORT_FILEFORMAT_JPG=1
    SUPPORT_FILEFORMAT_BMP=1)
```

The compile-time defines arrive before raylib's `config.h` is processed; the commented-out lines in `config.h` don't undefine them, so the `#if defined(SUPPORT_FILEFORMAT_JPG)` blocks compile in. Then `cmake -B build` + a `--clean-first` rebuild of the raylib target picks them up (incremental builds won't — raylib's `.o` files are already cached). General rule for raylib: if `ExportImage` / `LoadImage` silently refuses a format you "thought it supports," check the `SUPPORT_FILEFORMAT_*` matrix in raylib's `config.h` first.

**Files:** `CMakeLists.txt`.

**Phase:** v1.1 polish (multi-format export).

---

## 2026-05-18 — Sidebar hint text disappears past the right edge of the panel

**Symptom:** After the layout refactor to 320 px-wide sidebars, the `TextDisabled` hint blocks in the CustomChord and Parameters panels (e.g. "add nail: left-click on canvas drops a nail", "scroll wheel on slider: ± step  Shift: ×10  Ctrl: ×0.1", "keys 1-4: switch generator  F or 0: reset camera") got chopped at the panel edge — only the leading portion was visible. A couple of long button labels ("Clear chord colours (use style colormap)") and checkbox labels ("plotter mode (single colour, no opacity, sorted)") were similarly clipped.

**Root cause:** ImGui's `Text`/`TextDisabled`/etc. render at the widget's natural width by default — they do **not** word-wrap unless you push a `TextWrapPos`. The previous floating-window layout had been wide enough that no one noticed. Buttons and checkbox labels are worse: they always clip rather than wrap, with no equivalent push to make them wrap.

**Fix:** Push `ImGui::PushTextWrapPos(0.0f)` (the literal value 0 means "wrap at the end of the current window") immediately after `ImGui::Begin` in `render_left_sidebar` / `render_right_sidebar`; pop before `ImGui::End`. That covers every `Text*` call inside any tab. Two helpers `push_panel_wrap` / `pop_panel_wrap` document the intent. For the labels that don't wrap (buttons, checkboxes) the fix is to shorten the string or split into checkbox + companion `TextDisabled` on the same line via `ImGui::SameLine`; that disabled-text portion then wraps cleanly. Rule of thumb that came out of this: design sidebar content to fit the **default 320 px width**; if a label can't, consider whether the explanation belongs as a tooltip instead.

**Files:** `app/main.cpp`.

**Phase:** v1.1 polish.

---

## 2026-05-18 — AppImage launches but bundled presets are missing

**Symptom:** Inside a packaged AppImage, the Presets panel's "Bundled" section was empty even though `presets/*.json` sat at `Caustic.AppDir/usr/share/caustic/presets/`. User presets (under `$XDG_CONFIG_HOME/caustic/presets/`) showed up fine.

**Root cause:** `refresh_preset_lists` called `list_presets("presets")` — a path relative to the **current working directory**. From a development tree the cwd is the repo root and `./presets/` resolves correctly. From an AppImage the cwd is wherever the user double-clicked (typically `~` or `~/Downloads`), where no `./presets/` exists.

**Fix:** Add an `executable_dir()` helper that resolves `/proc/self/exe` (Linux-only path; falls back to an empty path on read failure or other platforms) and a `find_bundled_preset_dir()` that tries the candidates in order: `./presets` (dev convenience), `<exe>/../share/caustic/presets` (the AppImage / standard install layout), `<exe>/presets` (portable side-by-side). Returns the first that exists. `list_presets` already tolerates an empty path so the chain degrades gracefully. The AppImage's `build-appimage.sh` installs to `usr/share/caustic/presets`, so this lookup finds them on every AppImage launch. The same pattern works for future system installs.

**Files:** `app/main.cpp`, `packaging/appimage/build-appimage.sh`.

**Phase:** Phase 12 (release work, AppImage packaging skeleton).

---

## 2026-05-18 — CustomChord nails won't align with the grid after reloading a preset

**Symptom:** Build a CustomChord layer against `snap_to_grid` with `spacing = 0.05`, save the preset, reopen it later — the original nails were still there at their saved positions, but new nails snapped to a different grid (`spacing = 0.1`, the default) and wouldn't align with the saved set. Reproducible across every CustomChord save/load.

**Root cause:** The editor's grid state (`nail_grid_mode`, `nail_grid_spacing`, `nail_grid_polar_spokes`, plus the visible/snap toggles) lived only on `AppState` — pure UI state, never serialised. Loading a preset replaced `state.preset` but left those AppState fields at their compile-time defaults, so any nails authored against a non-default grid couldn't be extended afterward.

**Fix:** Move the grid state into the preset itself. New `caustic::EditorGridMode { Rectangular, Polar }` enum and `caustic::EditorGrid { mode, spacing, polar_spokes, visible, snap }` struct in `core/include/caustic/preset.hpp`, attached to `Preset` as `editor_grid`. New `editor_grid` JSON block in `preset_io.hpp` with default-fallback for older bundled presets that don't have it. UI rewired to read/write `state.preset.editor_grid.*` directly (no AppState mirror), so the source of truth is one place and save/load round-trips it for free. Added doctest coverage for the round-trip and the legacy-preset fallback.

**Files:** `core/include/caustic/preset.hpp`, `core/include/caustic/preset_io.hpp`, `app/main.cpp`, `tests/test_preset_io.cpp`.

**Phase:** v1.1 polish (nice-to-have batch).

---

## 2026-05-10 — Segfault on app exit after `Window closed successfully`

**Symptom:** `./build/app/caustic` ran fine, but on close raylib's log ended with `INFO: Window closed successfully` followed by `Segmentation fault (core dumped)`. No segfault inside the main loop.

**Root cause:** `caustic::RaylibRenderer`'s destructor calls `UnloadRenderTexture(canvas_)`. The renderer was declared as a local in `main()` and went out of scope **after** `CloseWindow()` had torn down the OpenGL context. The destructor then dereferenced a freed GL handle.

**Fix:** Wrap `renderer` and `AppState` in an inner scope inside `main()` so their destructors fire **before** `rlImGuiShutdown()` and `CloseWindow()`. Also added a defensive `try/catch` around the per-frame redraw call so a thrown `std::exception` from `geometry_from_spec` becomes a status-bar message instead of an abort.

**Files:** `app/main.cpp`.

**Phase:** 9 Stage B.

---

## 2026-05-10 — Bundled preset smoke test expected `p.version == 1` after auto-promote

**Symptom:** After bumping the preset schema to v2 (multi-layer scenes), the existing `bundled preset files parse cleanly` doctest case failed on all 5 bundled presets with `CHECK( p.version == 1 ) is NOT correct!`.

**Root cause:** The new `from_json` auto-promotes v1 presets to v2 in memory (sets `p.version = 2` after wrapping the old generator/style into a single Layer). The test assertion hadn't been updated.

**Fix:** Update the test to expect `p.version == 2` regardless of on-disk version, and also assert `scene.layers` is non-empty. Migration of the bundled presets to on-disk v2 followed in a separate commit.

**Files:** `tests/test_preset_io.cpp`.

**Phase:** 9 Stage A.

---

## 2026-05-10 — `Preset.scene.layers[0]` out-of-bounds on default construction

**Symptom:** Tests and UI code that accessed `preset.scene.layers[0]` on a freshly default-constructed `Preset` segfaulted (empty vector indexed).

**Root cause:** New `Scene` struct had `std::vector<Layer> layers;` (default-empty). The UI and tests assumed at least one layer exists.

**Fix:** Default-construct `Scene` with one empty Layer: `std::vector<Layer> layers = {Layer{}};`. The app's `current_layer()` accessor also clamps the index and lazily inserts a Layer if the vector is empty, as belt-and-suspenders.

**Files:** `core/include/caustic/preset.hpp`, `app/main.cpp`.

**Phase:** 9 Stage A.

---

## 2026-05-10 — `\xc2\xa75` invalid hex escape in CLI help text

**Symptom:** Compiler error: `Hex escape sequence out of range` on the CLI help string `"Exit codes (SPEC.md \xc2\xa75):\n"`.

**Root cause:** C++ hex escapes (`\x...`) are **greedy** — they consume every following hex digit. `\xa75` was parsed as a single 12-bit value, which overflows a `char`. The intent was the UTF-8 bytes for `§` (`\xc2\xa7`) followed by the literal character `5`.

**Fix:** Split the literal into adjacent string fragments so the `5` is its own token: `"\xc2\xa7" "5"`. C++ concatenates adjacent string literals automatically.

**Files:** `cli/main.cpp`.

**Phase:** 7.

---

## 2026-05-10 — `<fstream>` not transitively included via `<filesystem>`

**Symptom:** Test failed to compile with `variable 'std::ifstream f' has initializer but incomplete type`. The file included `<filesystem>` but used `std::ifstream`.

**Root cause:** `<filesystem>` does **not** transitively include `<fstream>`. The previous build had picked up `<fstream>` from another header chain by accident.

**Fix:** Add `#include <fstream>` explicitly.

**Files:** `tests/test_svg_renderer.cpp`.

**Phase:** 6.

---

## 2026-05-10 — `caustic::Color` shadows raylib's `::Color` inside the `caustic` namespace

**Symptom:** Inside `namespace caustic`, the raylib `WHITE` macro (which expands to `Color{255, 255, 255, 255}`) constructed a `caustic::Color` instead of a raylib `::Color`, then failed to pass to `DrawTexturePro` which expects `::Color`. Error: `could not convert 'caustic::Color((double)255, ...)' from 'caustic::Color' to 'Color'`.

**Root cause:** Caustic's RGBA-double `Color` struct lives in `namespace caustic`. Inside that namespace, unqualified `Color` resolves to `caustic::Color`, shadowing the global `::Color` from raylib's `<raylib.h>`. The `WHITE` / `BLACK` / `RED` raylib macros all expand to `CLITERAL(Color){...}` and trip this.

**Fix:** Inside `namespace caustic`, explicitly qualify raylib's Color as `::Color` whenever needed: `DrawTexturePro(canvas_.texture, src, dst, {0.0f, 0.0f}, 0.0f, ::Color{255, 255, 255, 255});`. The shorter alternative would be to rename `caustic::Color` → `caustic::Rgba` and avoid the shadowing entirely; rejected because ARCHITECTURE.md and SPEC.md both use "Color" as the canonical name.

**Files:** `render/raylib_renderer.cpp`.

**Phase:** 3.

---

## 2026-05-10 — Closed-curve color and stroke-width "seam" at t=1 → t=0 wrap-around

**Symptom:** Trochoids, Lissajous, and other closed curves rendered with `HsvSweep` or `LinearGradient` colormaps showed a visible color discontinuity where the curve closed back on itself — the figure looked like it had a starting/ending point even though geometrically it's a single closed loop.

**Root cause:** The colormap and stroke-width indexers map `t ∈ [0, 1]` linearly to a colormap range. At `t=0` they return colormap.at(0) (say red); at `t=1` they return colormap.at(1) (say purple). For closed curves, the renderer draws back-to-back segments at t=1 and t=0, so the user sees red touching purple.

**Fix:** Added `Style.cyclic` flag (default off). When set, the renderer triangle-wave-remaps `t` through `1 - |2t - 1|` before colormap and width lookup. Both endpoints (t=0 and t=1) now produce the same color (colormap.at(0)), and the gradient sweeps out to colormap.at(1) at t=0.5 and back. Modular chord (a discrete chord set with no continuity) leaves it off.

**Files:** `core/include/caustic/style.hpp`, `render/raylib_renderer.cpp`, plus `render_svg` followed the same pattern.

**Phase:** 3.

---

## 2026-05-10 — rlImGui `main` requires ImGui v1.92 texture-manager APIs

**Symptom:** First app build failed with a cascade of errors in `_deps/rlimgui-src/rlImGui.cpp`: `'GetPlatformIO' is not a member of 'ImGui'`, `'ImGuiBackendFlags_RendererHasTextures' was not declared`, `'ImTextureData' was not declared`, etc.

**Root cause:** rlImGui's `main` branch had been updated to use ImGui's v1.92 texture-manager refactor (PlatformIO-based render state, the new `ImTextureData` lifecycle). I had pinned rlImGui to `main` but Dear ImGui to `v1.90.4`, which predates that API.

**Fix:** Bump all three pins to a coherent tagged matrix:

- raylib `5.0` → `6.0`
- rlImGui `main` → `Raylib_6_0` (rlImGui ships `Raylib_*` tags named after the raylib release they target)
- Dear ImGui `v1.90.4` → `v1.92.7`

rlImGui and raylib must always move in lockstep per the architecture's "rlImGui version drift" pitfall.

**Files:** `CMakeLists.txt`, `BUILD.md` (pinned-version table).

**Phase:** 0.

---

## 2026-05-10 — Stale CMake cache after raylib 5.0 → 6.0 bump

**Symptom:** After bumping raylib's `GIT_TAG`, `cmake -B build` failed with `GLFW_USE_WAYLAND has been removed; delete the CMake cache and set GLFW_BUILD_WAYLAND and GLFW_BUILD_X11 instead`.

**Root cause:** raylib 6.0 bundles a newer GLFW that renamed its Wayland/X11 options. The existing `build/CMakeCache.txt` still had the old option names.

**Fix:** `rm -rf build && cmake -B build`. Whenever a FetchContent dependency changes major version, expect a `rm -rf build` to be needed.

**Files:** (operational).

**Phase:** 0.

---

## 2026-05-10 — `imgui.ini` thrashing in `git status`

**Symptom:** `git status` showed `imgui.ini` as modified after every app run, even with no intentional changes. Every commit risked dragging an unrelated layout state along.

**Root cause:** Dear ImGui writes window positions, sizes, and dock state to `imgui.ini` in the working directory on shutdown. It rewrites on any window move. It was committed in the first scaffolding commit before this behavior was understood.

**Fix:** `git rm --cached imgui.ini` + add `imgui.ini` to `.gitignore`. Per-user layout choices stay local.

**Files:** `.gitignore`, removed `imgui.ini` from tracking.

**Phase:** post-Phase 4 cleanup.

---

## 2026-05-10 — Hypotrochoid `r = R` produces an empty render

**Symptom:** Dragging the hypotrochoid `r` slider until it matched `R` caused the canvas to go blank. No error, just nothing visible.

**Root cause:** The hypotrochoid math `x(t) = (R - r)cos(t) + d·cos((R - r)/r · t)` collapses to `x = d`, `y = 0` when `R == r` — the curve degenerates to a single point `(d, 0)`. SPEC.md §2.2 documents `r ∈ (0, R)` as the constraint, but the slider didn't enforce it.

**Fix:** UI: clamp `r ≤ R - 1` in the hypotrochoid panel; raise `R`'s min to 2 so at least one valid `r` always exists. When the user drags `R` down, automatically clamp `r` to the new ceiling. The kernel still accepts any `r` (it's the UI that prevents the degenerate case).

**Files:** `app/main.cpp`.

**Phase:** 4 polish.

---

## 2026-05-10 — Irrational R/r or a/b produces "broken arcs" instead of clean curves

**Symptom:** Dragging hypotrochoid/epitrochoid `R` or `r`, or Lissajous `a` or `b`, through non-integer values produced patterns that looked like disconnected arcs rather than closed figures.

**Root cause:** Trochoids close at `T = 2π · r / gcd(R, r)` for rational R/r; irrational R/r fills an annulus over infinitely many revolutions. My curve sampler used a finite t range based on the *rounded* integer R, r — so for, say, `R=5.796, r=5.060`, the curve drew ~5 revolutions but never closed (since the actual ratio isn't 6/5). The visual result was a sparse fill that read as broken arcs.

**Fix:** Make the `R`, `r` sliders (and Lissajous `a`, `b`) **integer-only** so the ratio is always a small-denominator rational and the curve closes after `gcd` revolutions. `d`, `A`, `B`, `φ` remain continuous (they don't affect closure). The architecture's "drag through irrational a/b to watch Lissajous precess" killer demo is therefore unavailable until a future "sample-over-many-revolutions" toggle is added — recorded as a Phase 4 deviation in CLAUDE.md.

**Files:** `app/main.cpp`.

**Phase:** 4 polish.

---

## Conventions for this file

- Newest entry at the top.
- Date in ISO format (`YYYY-MM-DD`).
- Short, scannable title — symptom-first wording so search-by-error-message works.
- The **Fix** section names the actual code path that changed, not just "added a check."
- Link or reference the relevant Phase so reading along with ROADMAP makes sense.
- Don't log routine debugging — log things that another person (or a session restart) would benefit from knowing about.
