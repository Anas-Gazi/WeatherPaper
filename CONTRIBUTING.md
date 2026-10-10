# Contributing to WeatherPaper

Thanks for looking at this. See [`ARCHITECTURE.md`](ARCHITECTURE.md) first
for the module map and, importantly, the **Verification status** table —
it tells you honestly what's tested and what needs your help.

## Building on Linux

Tested on Ubuntu 24.04 with GCC 13. You'll need:

```bash
sudo apt-get update
sudo apt-get install -y cmake build-essential libssl-dev libcurl4-openssl-dev
# Optional, for animated wallpaper decode support:
sudo apt-get install -y libavformat-dev libavcodec-dev libswscale-dev libavutil-dev
# Optional, for the tray icon (WEATHERPAPER_BUILD_TRAY_UI=ON):
sudo apt-get install -y libayatana-appindicator3-dev libgtk-3-dev
# Optional, for the settings window (WEATHERPAPER_BUILD_SETTINGS_UI=ON):
sudo apt-get install -y qt6-base-dev
```

Then:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DWEATHERPAPER_WITH_FFMPEG=ON \
  -DWEATHERPAPER_BUILD_TRAY_UI=ON \
  -DWEATHERPAPER_BUILD_SETTINGS_UI=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
```

Every option above defaults `OFF` except `WEATHERPAPER_BUILD_APP` and
`WEATHERPAPER_WITH_CURL`/`WEATHERPAPER_WITH_OPENSSL` — the core engine, all
its tests, and the daemon build with nothing but `cmake` + a C++20 compiler
+ libcurl + OpenSSL. FFmpeg/tray/settings are additive.

Run the daemon directly from the build tree (it points at the in-tree
bundled theme automatically — see `src/app/CMakeLists.txt`):

```bash
./build/src/app/weatherpaperd --once   # one pipeline pass, then exit
./build/src/app/weatherpaperd          # normal long-running daemon (Ctrl+C to quit)
```

## Building on Windows

**Status:** The Windows application and console-free launcher have been built locally using MinGW-w64, Qt 6, CMake, and vcpkg. The installed application launcher has also been launched successfully. This confirms that the tested build can start; it does not establish that every Windows-specific feature or the final release packages have been fully verified.

### Prerequisites

- Windows 10 or Windows 11
- CMake 3.20 or newer
- A C++20 compiler: MSVC or MinGW-w64
- Qt 6 Widgets and the matching Qt deployment tools for the GUI
- vcpkg with dependencies built for the same compiler and architecture
- The Windows SDK for the relevant Win32 APIs

**Toolchain compatibility matters:** Do not mix MSVC and MinGW binaries, Qt installations, or vcpkg libraries in the same build or release package.

### Configure and build with MinGW-w64

The following is a template. Replace the Qt and vcpkg paths with the locations on your machine.

```powershell
cmake -S . -B build-windows `
  -G "MinGW Makefiles" `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="E:/path/to/Qt/mingw_64" `
  -DCMAKE_TOOLCHAIN_FILE="E:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-mingw-dynamic `
  -DWEATHERPAPER_BUILD_SETTINGS_UI=ON `
  -DWEATHERPAPER_BUILD_TRAY_UI=ON `
  -DWEATHERPAPER_BUILD_TESTS=OFF

cmake --build build-windows --parallel
```

Ensure that the MinGW compiler and make executable are available to CMake. Use a separate build directory for each compiler and toolchain.

For MSVC, configure a separate build directory with a Visual Studio generator, an MSVC-compatible Qt installation, and an MSVC-compatible vcpkg triplet.

### Windows runtime deployment

A distributable GUI build needs the application binaries, their matching runtime dependencies, required Qt plugins, and the bundled theme assets.

Use `windeployqt` from the same Qt installation used to compile the application. Verify the resulting package on a Windows environment that does not rely on development-tool paths or DLLs already present on the developer's machine.

The repository's Windows packaging files are under `packaging/windows/`. Before publishing a release, verify that the packaging script stages both `WeatherPaper.exe` and `weatherpaperd.exe`, includes the required runtime and theme files, and reports failures correctly.

### Verification checklist

Test the following separately; a successful compile is not sufficient:

- The `WeatherPaper.exe` launcher opens without an unwanted console window.
- The main application starts and finds its bundled theme assets.
- Wallpaper-setting behavior works on the target Windows version.
- The settings window and system-tray interactions work.
- Weather refresh and network error handling behave as expected.
- Optional startup configuration and its removal work correctly.
- The setup installer and portable ZIP both work when extracted or installed outside the development tree.
- Uninstallation removes the application's startup registration when appropriate.

### Reporting a Windows build issue

Please include your Windows version, compiler and version, Qt version, vcpkg triplet, exact CMake command, and the complete error output. Reports about unverified features are useful even when the core application compiles successfully.


## Adding a new Linux desktop-environment backend

This is explicitly the area of the codebase most likely to receive
community PRs (Linux DE fragmentation is inherent to the problem). Good
news: it's also the most test-friendly part of the whole project.

1. Open `src/platform/linux/include/weatherpaper/platform_linux/backend.hpp`
   and add your DE to the `DesktopEnvironment` enum.
2. In `src/platform/linux/src/backend.cpp`:
   - Add a detection branch in `detect_desktop_environment()` — usually a
     new `icontains(*xcd, "yourde")` check against `$XDG_CURRENT_DESKTOP`.
   - Add a `build_your_de(...)` function returning
     `std::vector<WallpaperCommand>` — **always** as `{program, argv...}`,
     **never** as a concatenated shell string (see `ARCHITECTURE.md` section 4 —
     this is a hard security requirement, not a style preference).
   - Wire it into the `switch` in `build_static_image_command()` (and
     `build_video_command()` if your DE has native video-wallpaper support).
   - If your DE supports per-monitor wallpapers, add it to
     `supports_per_monitor()`.
3. Add tests to `tests/platform_linux/test_platform_linux_backend.cpp`
   mirroring the existing ones — you do NOT need a real installation of
   your DE to test this: `detect_desktop_environment()` takes an
   `IEnvReader`, so tests just fake the environment variables (see
   `FakeEnvReader` in that file) and command-building tests just inspect
   the returned `WallpaperCommand` argv, no process ever actually runs.
4. Run `ctest --test-dir build -R platform_linux_backend --output-on-failure`
   — if it passes, your backend is logically correct even before you've
   tested it on real hardware. Then test on real hardware and report back
   in your PR whether the actual OS-level command worked as expected
   (sometimes upstream tool flags/behavior drift between distro versions —
   that's useful information even if your first attempt isn't perfect).

## Adding a new bundled or downloadable theme pack

Theme packs — bundled or remote — share one format:
`tag_system`'s JSON schema (see `tag_system.hpp`'s `AssetRecord`).

**Bundled** (`assets/default_theme/` is the only one shipped today):

1. Add your image/video files under `assets/default_theme/images/`.
2. Add a matching entry to `assets/default_theme/tags.json`:
   ```json
   { "id": "your_id", "file": "images/your_file.jpg", "type": "image",
     "fit_mode": "fill", "tags": ["rain", "night"] }
   ```
   `id` must be unique within the file. `tags` should include at least a
   weather-condition tag and/or a time-of-day tag (see
   `tag_system::standard_tags` for the exact vocabulary) so
   `wallpaper_engine_core::Resolver` can find it.
3. `src/app/main.cpp` loads this file automatically at startup — no other
   wiring needed.

**Downloadable** (via `asset_manager::ThemePackManager`): host a
`ThemePackManifest`-shaped JSON (see `asset_manager.hpp`) listing every
file's URL + SHA-256, plus a `tags.json` in the same shape as above, and
add an entry to your catalog JSON (`RemoteCatalog`/`CatalogEntry` shape)
pointing at that manifest with its own checksum. `ThemePackManager::install()`
handles the rest (download, verify, write, merge tags) — see
`tests/asset_manager/test_asset_manager.cpp`'s
`"ThemePackManager::install downloads, verifies..."` test for a complete
worked example you can copy the shape from.

## Implementing `IVideoSurface` (the top-priority follow-up)

`render_engine.hpp`'s `IVideoSurface` interface is specified but has no
concrete implementation (see `ARCHITECTURE.md` section 6 for why). What's
needed:

- **Windows**: the "WorkerW technique" — send `0x052C` to Progman to spawn
  a WorkerW window behind desktop icons, then create your own child window
  inside it and blit `FrameBuffer`s (from `render_engine::blit_into`) into
  it. Lively Wallpaper's source is a good reference for the exact Win32
  sequence (see the original spec's "Reference Concepts" — don't copy code,
  just study the technique).
- **Linux X11**: create an override-redirect window sized to the root
  window, lowered below icons via `_NET_CLIENT_LIST`/window-manager-specific
  hints, or simpler: set the window as the root window's background pixmap
  directly via Xlib (`XSetWindowBackgroundPixmap` on the root window) for
  static-ish updates.
- **Linux Wayland**: you likely don't need to implement this at all —
  `platform_linux::build_video_command()` already delegates to `mpvpaper`
  for Hyprland/Sway, which handles its own layer-shell surface. Focus
  effort on X11/Windows.

Once you have a working `IVideoSurface`, wire it into a new
`render_engine::RenderLoop` class that ties `IVideoDecoder::next_frame()` ->
`scaling_and_fit::compute_fit()` -> `render_engine::blit_into()` ->
`IVideoSurface::present()`, gated by `should_render_animated_frame()`
(already implemented and tested) and driven by
`platform_common::IPlatformHooks::pump_events()` on a timer matching the
video's frame rate.

## Code style

- C++20, four-space indent, no tabs.
- Every module: public header with design-rationale comments, `.cpp` with
  `ASSUMPTION:` comments wherever a non-obvious decision was made, its own
  `CMakeLists.txt`, and a `doctest` suite for anything that isn't pure OS
  glue.
- Prefer a named `struct`/`enum class` returned by value over an
  out-parameter or a `bool` + separate getter.
- No `system()`/`popen()` with anything derived from user/file/network
  input — see `ARCHITECTURE.md` section 4. If you're tempted, look at how
  `platform_linux::WallpaperCommand` + `execvp` solves the same problem.

## Running just one test suite

```bash
ctest --test-dir build -R tag_system --output-on-failure   # by module name
./build/tests/tag_system/test_tag_system --test-case="upsert*"  # doctest filter
```

## Reporting a Windows build issue

Please include: your compiler (MSVC version or MinGW-w64 version), the
exact `cmake` command you ran, and the full error output. Given the
verification-status caveat above, "it doesn't compile" reports are
expected and genuinely useful — thank you in advance.
