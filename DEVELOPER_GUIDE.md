# WeatherPaper — Developer Guide

This is the fast-onboarding companion to [`ARCHITECTURE.md`](ARCHITECTURE.md)
(the *why*) and [`CONTRIBUTING.md`](CONTRIBUTING.md) (the *how to build*).
Read this if your question is **"where in this codebase do I find/change
X?"** — it's organized as a lookup table, not a narrative.

## First five minutes

```bash
git clone <this repo>
cd WeatherPaper
cmake -S . -B build -DWEATHERPAPER_WITH_FFMPEG=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure   # should print "100% tests passed"
./build/src/app/weatherpaperd --once         # run the real pipeline once
```

If all of that works, you have a correct, fully-functional dev environment
for everything except the Windows backend (needs Windows) and interactive
tray/settings-window testing (needs a real desktop session).

## "Where do I find..." lookup table

| I want to... | Look at |
|---|---|
| Change how weather conditions map to tags | `src/modules/weather_fetch/src/weather_fetch.cpp` — `wmo_weathercode_to_condition()` / `owm_id_to_condition()` |
| Change the morning/day/evening/night time boundaries | `src/modules/time_of_day/include/.../time_of_day.hpp` — `BucketWidths` |
| Add a new fit mode (beyond Fill/Fit/Stretch/Center/Tile) | `src/modules/scaling_and_fit/` — add to `FitMode` enum, `compute_fit()`, and every platform's fit-mode mapping table (`backend.cpp` for Linux, `wallpaper_windows.cpp` for Windows) |
| Change which asset gets picked when several match the same tags | `src/modules/tag_system/include/.../tag_system.hpp` — `SelectionPolicy` and `TagIndex::resolve_selection()` |
| Add a new Linux desktop environment | `src/platform/linux/src/backend.cpp` — see `CONTRIBUTING.md`'s step-by-step |
| Change how Windows sets the wallpaper | `src/platform/windows/src/wallpaper_windows.cpp` |
| Change the crossfade transition duration/curve | `src/modules/wallpaper_engine_core/include/.../wallpaper_engine_core.hpp` — `CrossfadeScheduler` |
| Add a new bundled wallpaper | `assets/default_theme/tags.json` + `assets/default_theme/images/` — see `CONTRIBUTING.md` |
| Add a downloadable theme pack | Host a manifest matching `asset_manager.hpp`'s `ThemePackManifest`; see `CONTRIBUTING.md` |
| Change checksum/signature algorithm | `src/modules/asset_manager/src/asset_manager.cpp` — `sha256_hex_of_bytes()`, `verify_rsa_sha256_signature()` (also used by `updater`) |
| Change the update-check/rollback behavior | `src/modules/updater/` |
| Change severe-weather alert thresholds/wording | `src/modules/weather_fetch/src/weather_fetch.cpp`'s `is_severe()` (thresholds) and `src/modules/notify/src/notify.cpp`'s `format_severe_weather_notification()` (wording) |
| Add a tray menu item | `src/modules/tray_ui/include/.../tray_ui.hpp`'s `TrayCallbacks`, then wire it in both `tray_ui_linux.cpp` and `tray_ui_windows.cpp` |
| Add a settings screen | `src/modules/settings_ui/src/settings_ui.cpp` — add a `build_your_tab()` method and register it in `build_ui()` |
| Change config file locations | `src/modules/config/src/config.cpp` — `default_config_dir()` / `default_cache_dir()` / `default_data_dir()` |
| Wire up the actual video-wallpaper renderer | `src/modules/render_engine/include/.../render_engine.hpp`'s `IVideoSurface` — **currently unimplemented**, see `CONTRIBUTING.md` |
| Change how the main loop schedules polling/pause/resume | `src/app/main.cpp` |

## The one rule that matters most

**Dependency arrows point from "networked/OS-specific" toward "pure
logic", never the reverse.** Concretely:

- `weather_fetch` depends on `wallpaper_engine_core` (for `Condition`), not
  the other way around.
- `platform_windows`/`platform_linux` depend on `platform_common`
  (interfaces), never the reverse, and `render_engine`/`asset_manager`/etc.
  depend on `platform_common`'s interfaces too, never a concrete backend
  directly.
- `tag_system` has zero knowledge of HTTP, checksums, or the OS.

If you find yourself wanting to `#include` a networking or OS header from
inside `time_of_day`, `scaling_and_fit`, `tag_system`, or
`wallpaper_engine_core`, stop — that's a sign the thing you're adding
belongs in a higher layer instead. This rule is *why* 129 tests run with
zero network access and zero display server; breaking it would quietly
start requiring a live weather API or a real desktop to run the test
suite.

## Adding a brand-new module

1. `src/modules/<name>/{include/weatherpaper/<name>/,src/}` + its own
   `CMakeLists.txt` (copy an existing simple one, e.g. `scaling_and_fit`'s,
   as a template).
2. Public header: one `.hpp` under `include/weatherpaper/<name>/`, with a
   comment block explaining the module's layer (pure logic? network? OS
   glue?) and its dependencies, matching the style every existing module
   uses.
3. `add_subdirectory(src/modules/<name>)` in the top-level `CMakeLists.txt`,
   in the right dependency-order position (see `ARCHITECTURE.md`'s table).
4. `tests/<name>/test_<name>.cpp` + `CMakeLists.txt`, and
   `add_subdirectory(<name>)` in `tests/CMakeLists.txt`.
5. Update `ARCHITECTURE.md`'s module table.

## Testing philosophy in one paragraph

Every side effect (network, filesystem beyond a temp file, OS API, process
execution) goes behind a small interface (`IHttpClient`, `IEnvReader`,
`IPlatformWallpaper`, ...). Production code gets a real implementation;
tests get a fake one that returns canned data. The *decision logic* (which
condition maps to which tag, which command a given desktop environment
needs, whether a checksum matches) is what actually gets tested — the thin
real-I/O implementations are deliberately small enough that bugs in them
are easy to spot by inspection, and the interfaces they implement are
exactly what a future contributor swaps out (see
`platform_linux::IProcessRunner`, `weather_fetch::IHttpClient`) without
touching anything upstream.

## Build flags cheat-sheet

| Flag | Default | Effect |
|---|---|---|
| `WEATHERPAPER_BUILD_TESTS` | `ON` | Build the `tests/` doctest suites |
| `WEATHERPAPER_BUILD_APP` | `ON` | Build `weatherpaperd` |
| `WEATHERPAPER_BUILD_TRAY_UI` | `OFF` | Build the tray icon (needs GTK3+AppIndicator3 on Linux) |
| `WEATHERPAPER_BUILD_SETTINGS_UI` | `OFF` | Build the settings window (needs Qt6 Widgets) |
| `WEATHERPAPER_WITH_CURL` | `ON` | Real HTTP transport for weather/updates/theme downloads |
| `WEATHERPAPER_WITH_OPENSSL` | `ON` | Real SHA-256/RSA verification (fails closed without it) |
| `WEATHERPAPER_WITH_FFMPEG` | `OFF` | Real video decode for animated wallpapers |

## Who to ask

There's no live team behind this snapshot — treat `ARCHITECTURE.md`'s
"Known limitations" list as the standing backlog, and every module's
`ASSUMPTION:` comments as the design-review notes you'd otherwise get from
a colleague.
