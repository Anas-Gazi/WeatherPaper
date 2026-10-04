# WeatherPaper — Architecture

This document explains how WeatherPaper is put together: module boundaries,
dependency direction, why certain design decisions were made, and — just as
important — **what has and hasn't been verified**, so a new contributor
never has to guess which parts are load-bearing and which are drafts.

If you're looking for *how to build/contribute*, see [`CONTRIBUTING.md`](CONTRIBUTING.md).
If you're looking for *how to use the app*, see [`USERGUIDE.md`](USERGUIDE.md).

---

## 1. The big picture

WeatherPaper turns `(weather condition, time of day)` into `(a file path,
a fit mode)`, then hands that to the operating system (or, for video, to
its own renderer) to actually display. Everything else — theme packs,
updates, the tray icon, the settings window — exists to *feed that pipeline
data* or *let the user configure how it behaves*.

```
                 +--------------+     +---------------+
                 | weather_fetch|     |  time_of_day   |  (pure, no deps)
                 +------+-------+     +--------+------+
                        | Condition            | Bucket
                        +----------+-----------+
                                   v
                    +----------------------------+
                    |   wallpaper_engine_core     |  Resolver, CrossfadeScheduler
                    |  (Condition lives here!)    |
                    +--------------+-------------+
                                   | queries
                                   v
                    +----------------------------+      +------------------+
                    |        tag_system           |<-----|  asset_manager   | installs/
                    |   (TagIndex, AssetRecord)   |      |  (theme packs)   | uninstalls
                    +--------------+-------------+      +------------------+
                                   | ResolvedWallpaper
                                   v
                    +----------------------------+
                    |  scaling_and_fit (geometry) |
                    +--------------+-------------+
                                   v
                    +----------------------------+
                    |       render_engine         |  static: OS API
                    |                              |  video:  own compositor
                    +--------------+-------------+
                                   v
                    +----------------------------+
                    |       platform_common       |  interface only
                    |  platform_windows / _linux  |  concrete backends
                    +----------------------------+
```

`config`, `updater`, `notify`, `tray_ui`, and `settings_ui` sit alongside
this pipeline rather than inside it — they configure it, keep it updated,
surface alerts from it, and give the user a UI onto it.

## 2. Module-by-module

Every module lives at `src/modules/<name>/` with its own `CMakeLists.txt`,
a public header under `include/weatherpaper/<name>/`, and a private
implementation under `src/`. The two platform backends live at
`src/platform/{windows,linux}/` instead, since they're OS-selected rather
than always-built.

| # | Module | Depends on | What it owns |
|---|---|---|---|
| 1 | `time_of_day` | *(nothing)* | sunrise/sunset + now -> Morning/Day/Evening/Night |
| 2 | `scaling_and_fit` | *(nothing)* | Fill/Fit/Stretch/Center/Tile geometry math |
| 3 | `tag_system` | `scaling_and_fit` | the on-disk JSON asset/tag index, `TagIndex` |
| 4 | `wallpaper_engine_core` | `time_of_day`, `tag_system` | **`Condition` enum lives here**; `Resolver`, `CrossfadeScheduler` |
| 5 | `config` | `tag_system` | `AppConfig`, XDG/`%LOCALAPPDATA%` path resolution |
| 6 | `weather_fetch` | `wallpaper_engine_core` | `IHttpClient`, Open-Meteo/OWM providers, disk cache, backoff, seasonal fallback |
| 7 | `platform_common` | `scaling_and_fit`, `tag_system` | **interfaces only** — `IPlatformWallpaper`, `IPlatformHooks` |
| 8 | `platform_linux` | `platform_common` | DE detection, argv command building, process execution |
| 8b | `platform_windows` | `platform_common` | `IDesktopWallpaper` COM, WTS/power/network hooks |
| 9 | `render_engine` | `platform_common`, `scaling_and_fit`, `wallpaper_engine_core` | `FrameCompositor`, `IVideoDecoder` (FFmpeg), pause/resume logic |
| 10 | `asset_manager` | `tag_system`, `scaling_and_fit`, `weather_fetch` (reused `IHttpClient`) | checksum/signature verification, theme-pack install/uninstall, upload validation |
| 11 | `updater` | `weather_fetch`, `asset_manager` | semver compare, signed package staging, rollback |
| 12 | `notify` | `wallpaper_engine_core` | severe-weather toast formatting + OS notifiers |
| 13 | `tray_ui` | *(none — pure interface + native backends)* | AppIndicator3 (Linux) / `Shell_NotifyIcon` (Windows) |
| 14 | `settings_ui` | `config`, `tag_system`, `asset_manager` | Qt Widgets classic settings/gallery window |

### Why `Condition` lives in `wallpaper_engine_core`, not `weather_fetch`

This is the single most important dependency-direction decision in the
codebase. If `Condition` lived in `weather_fetch`, then
`wallpaper_engine_core` — meant to be pure, offline, trivially unit-tested
logic — would have to depend on the networking module just to know what a
"rainy" tag looks like. Instead, `weather_fetch` depends on
`wallpaper_engine_core` and returns its `Condition` type. Network -> pure
logic, never the reverse. The same reasoning is why `tag_system::TagIndex`
knows nothing about HTTP, and why `render_engine`'s pixel-compositing math
(`FrameCompositor`) doesn't touch `platform_common` at all.

### Why `IHttpClient` is reused across `weather_fetch`, `asset_manager`, and `updater`

Section 3.10 of the original spec is explicit: *"All update-related network
calls go through the same HTTPS + checksum/signature verification path as
asset_manager downloads — do not build two separate download/verify code
paths."* The cleanest way to guarantee that in code (not just in a code
review comment) is for `updater` and `asset_manager` to literally share one
`IHttpClient` interface and one set of `sha256_hex_of_bytes` /
`verify_rsa_sha256_signature` functions. That interface happens to be
declared in `weather_fetch` for historical reasons (it was needed there
first). **Known limitation:** a cleaner long-term home would be a dedicated
`http_client` module that all three depend on symmetrically — tracked below.

## 3. Testing strategy

Every pure-logic module (1–6, 9's compositor, 10, 11, and platform_linux's
`backend.cpp`) has a full `doctest`-based suite in `tests/<module>/`, run
with zero network access and zero display server — 129 test cases across 11
suites as of this writing, all passing (`ctest --output-on-failure`).

The trick that makes this possible for network- and OS-adjacent modules:
**every side effect goes through a small interface that tests can fake.**

- `weather_fetch::IHttpClient` -> tests use `MockHttpClient` with fixture JSON.
- `asset_manager`/`updater` reuse the same `IHttpClient` -> same trick applies.
- `platform_linux::IEnvReader` -> tests use `FakeEnvReader` instead of mutating
  real process environment variables.
- `platform_linux::IProcessRunner` -> **not** mocked in tests; instead, all
  the *decision* logic (which DE, which command, which argv) lives in
  `backend.cpp` and is tested directly, so the actual process-spawning code
  in `process_runner.cpp` is a thin, low-risk final step.

## 4. Security posture

- **HTTPS-only.** `CurlHttpClient::get()` refuses any URL not starting with
  `https://` before it ever calls libcurl. `asset_manager::parse_catalog`
  and `updater::parse_version_manifest` *also* drop any entry whose URL
  isn't HTTPS at parse time — defense in depth.
- **Checksums everywhere.** Every downloaded byte (theme-pack files, update
  packages) is SHA-256-verified against a manifest value before being
  trusted; the manifest itself is checksummed too.
- **Signatures for official content.** `asset_manager::verify_rsa_sha256_signature`
  is exercised in `tests/asset_manager` against a *real* OpenSSL-generated
  keypair and signature, not a hand-rolled fixture.
- **No shell, ever, for untrusted data.** `platform_linux`'s entire design
  (`WallpaperCommand{program, args}` + `execvp`) exists so a maliciously
  named file can never be shell-interpreted. See `test_platform_linux_backend.cpp`'s
  `"SECURITY: a malicious-looking filename..."` test. The one place that
  *does* use `popen()` (`hooks_linux.cpp`'s lock-state check) only ever runs
  fixed, literal, hardcoded command strings with zero interpolation — see
  that file's header comment for the full reasoning.
- **Path-traversal defense.** `asset_manager::is_safe_relative_path` rejects
  `..` components, absolute paths, and drive letters before any
  manifest-supplied relative path is joined with an install directory.
- **No location coordinates in logs.** `config::Location::latitude`/`longitude`
  are documented as never-log fields; only `display_name` should ever reach
  a log line or notification body — see `notify::SevereWeatherAlert`, which
  has no coordinate field at all, by construction.

## 5. Versioning

Three independent version numbers, deliberately not tied together:

- **Core engine**: `WEATHERPAPER_CORE_VERSION` in the top-level `CMakeLists.txt`.
- **Asset catalog**: `assets/default_theme/pack.json`'s `"version"` field,
  and each installed theme pack's own manifest version.
- **Settings UI**: intended to have its own version once packaged
  separately; currently tracks the core version since it ships in the same
  binary in this drop.

`updater::UpdateManager` treats each as an independent `component_id`
("core", "ui", "asset_catalog") that can be checked and applied
separately — see `updater.hpp`.

## 6. Verification status — please read before trusting a module

This was built and tested in a Linux-only sandbox with no Windows
toolchain and no live GUI/display-server session. Being precise about what
that means:

| Status | Modules |
|---|---|
| **Compiled + unit-tested in this environment** | `time_of_day`, `scaling_and_fit`, `tag_system`, `wallpaper_engine_core`, `config`, `weather_fetch`, `render_engine` (incl. real FFmpeg decode path), `asset_manager` (incl. real OpenSSL sign/verify round-trip), `updater`, `notify` (Linux notifier), `platform_linux` (backend/detection logic — the highest-risk module, fully tested) |
| **Compiled (real deps installed) but not run interactively** | `tray_ui` (AppIndicator3/GTK3 — compiles, needs a real tray host to click-test), `settings_ui` (Qt6 Widgets — compiles, needs a real X/Wayland session to click-test) |
| **Written against documented APIs, NOT compiled (no Windows SDK available)** | `platform_windows` (`IDesktopWallpaper`, WTS/power/network hooks), `notify`'s `WindowsToastNotifier`, `tray_ui`'s `WindowsTrayIcon` |
| **End-to-end pipeline run** | `src/app/main.cpp` (`weatherpaperd --once`) was executed in this sandbox: it loads the bundled theme, attempts a live weather fetch (fails — sandbox network is allow-listed and excludes `api.open-meteo.com`), falls back correctly, resolves an exact tag match, detects the (nonexistent, in-sandbox) desktop environment correctly as `Unknown`/`GenericX11` depending on env vars, and reports the expected graceful failure to actually set a wallpaper (there is no real desktop here). This proves the *wiring* is correct end to end. |

**The single highest-priority follow-up** is implementing
`render_engine::IVideoSurface` for real (the WorkerW-technique window on
Windows, an override-redirect/layer-shell surface on Linux) — see
`CONTRIBUTING.md` section "Implementing IVideoSurface". Everything upstream
of it (decoding, compositing math, pause/resume logic) is already built and
tested; only the final "put pixels on a window behind the desktop icons"
step is unimplemented, because it needs a real display server to develop
against interactively.

## 7. Known limitations (good first issues)

- **Windows build is unverified.** See section 6. First contribution: get it
  compiling in CI on `windows-latest`, fix whatever doesn't match reality.
- **`IVideoSurface` is unimplemented** (see section 6) — animated wallpapers
  decode and composite correctly in memory but never reach a screen yet.
- **XFCE per-monitor property paths** are version/monitor-name-dependent;
  `platform_linux/src/backend.cpp`'s `build_xfce()` targets the common
  default path only — see its inline comment.
- **Deepin (`DesktopEnvironment::Deepin`)** routes through a GNOME-schema
  best-effort guess, not a real Deepin-native API call — flagged in
  `backend.cpp`.
- **`platform_linux` monitor enumeration** returns one synthetic monitor;
  real `xrandr`/`hyprctl`/`swaymsg` output parsing isn't wired up (needs a
  stdout-capturing process runner, which `IProcessRunner` doesn't currently
  expose — see `wallpaper_linux.cpp`'s `enumerate_via_xrandr`).
- **Location auto-detection isn't implemented** — `src/app/main.cpp` uses a
  placeholder Dhaka coordinate when no config exists. Needs an IP-geolocation
  call or OS location-services integration.
- **`platform_linux`'s lock/network hooks are polling-based**, not
  D-Bus-signal-driven — see `hooks_linux.cpp`'s file header for the
  reasoning and what a fully event-driven version would need.
- **`IHttpClient`'s ideal home** is a dedicated `http_client` module shared
  symmetrically by `weather_fetch`/`asset_manager`/`updater`, not owned by
  `weather_fetch` — see section 2.
- **Bundled theme art is placeholder solid colors**, not real photography —
  see `assets/default_theme/README.md`.
