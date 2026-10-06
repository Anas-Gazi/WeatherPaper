# WeatherPaper

A cross-platform, modular, live weather-reactive wallpaper engine for
**Windows 10/11** and **Linux** (GNOME, KDE Plasma, XFCE, Cinnamon, MATE,
Hyprland, Sway). WeatherPaper automatically changes your desktop wallpaper
based on live weather conditions and real sunrise/sunset-based time of day
— and it's also a general-purpose static/video wallpaper engine you can
drive with your own tagged images and clips.

Built in C++20 for a small footprint (installer target: under 25MB), zero
busy-polling, and full offline unit-testability.

## What it does

- Watches live weather (via [Open-Meteo](https://open-meteo.com), no
  API key required) and switches wallpaper by condition — sunny, cloudy,
  rain, snow, storm, fog, clear.
- Computes time of day (morning/day/evening/night) from **real**
  sunrise/sunset for your location, not fixed clock hours.
- Supports static images and (opt-in, for capable hardware) short
  looped video wallpapers.
- Every wallpaper — bundled, downloaded, or your own — is matched by a
  simple tag system, editable in the settings UI.
- Downloadable theme packs, signed and checksum-verified.
- Works fully offline: caches the last known weather, and falls back to
  a bundled seasonal default if there's no cache at all.
- Idle CPU near 0% — everything is event/timer-driven, nothing
  busy-loops.
- HTTPS-only networking, checksummed + signature-verified downloads,
  no shell-injection surface anywhere file paths reach a subprocess.

## Status

This is a from-scratch build with a genuinely modular architecture and a
real, passing test suite — **129 unit tests across 11 suites, all green**
— covering every OS-independent module. The Linux platform backend (DE
detection + wallpaper-setting) is fully implemented and tested too, and the
whole pipeline has been run end-to-end in this repository's build
environment.

The Windows platform backend and the WorkerW-style video-wallpaper
compositor are written but **not yet verified on real hardware** — see
[`ARCHITECTURE.md`](ARCHITECTURE.md) section "Verification status" for the
exact, honest breakdown of what's tested versus what's a well-documented
first draft, and [`CONTRIBUTING.md`](CONTRIBUTING.md) for how to help close
that gap.

## Download & Installation

For full download links and step-by-step setup on Windows and Linux, read the **[Installation Guide (INSTALL.md)](INSTALL.md)**.

### Windows 10 & Windows 11
- **⚡ One-Click Download:** Download [**`WeatherPaper-v1.0.0-windows-x64.zip`**](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/WeatherPaper-v1.0.0-windows-x64.zip), extract with 1 click, and double-click `install-portable.bat` to launch and autostart on Windows.

### Linux (All Distributions / Ubuntu / Debian / Fedora / Arch)
- **⚡ One-Click Installer (`.run`):** Download [`weatherpaper-installer.run`](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/weatherpaper-installer.run), run it, and it automatically installs all files, desktop shortcuts, autostart, and launches WeatherPaper.
  ```bash
  chmod +x weatherpaper-installer.run
  ./weatherpaper-installer.run
  ```
- **Debian Package (.deb):**
  ```bash
  sudo dpkg -i weatherpaper-0.1.0-Linux.deb
  sudo apt-get install -f
  ```
- **Universal Installer (Arch, Fedora, openSUSE, or non-root):**
  ```bash
  ./packaging/linux/install.sh
  ```
- **Build Release .deb Package:**
  ```bash
  ./packaging/linux/build-deb.sh
  ```

## Quick start (build from source)

```bash
sudo apt-get install cmake build-essential libssl-dev libcurl4-openssl-dev qt6-base-dev libayatana-appindicator3-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DWEATHERPAPER_BUILD_SETTINGS_UI=ON -DWEATHERPAPER_BUILD_TRAY_UI=ON
cmake --build build -j$(nproc)
ctest --test-dir build --output-on-failure
./build/src/app/weatherpaperd --once
```

See [`CONTRIBUTING.md`](CONTRIBUTING.md) for the full build matrix
(Windows, optional FFmpeg/tray/settings-UI components) and
[`USERGUIDE.md`](USERGUIDE.md) for how to actually use the app once built.

## Documentation map

| Document | For |
|---|---|
| [`INSTALL.md`](INSTALL.md) | Download links, setup wizard, and manual installation guide |
| [`USERGUIDE.md`](USERGUIDE.md) | End users — configuring, adding wallpapers, auto-start |
| [`ARCHITECTURE.md`](ARCHITECTURE.md) | Contributors — module map, dependency rules, security posture, verification status |
| [`CONTRIBUTING.md`](CONTRIBUTING.md) | Contributors — build instructions, adding a Linux DE backend, adding a theme pack |
| [`DEVELOPER_GUIDE.md`](DEVELOPER_GUIDE.md) | New contributors — "where do I find X", "how do I extend Y" quick-reference |

## Repository layout

```
WeatherPaper/
├── src/
│   ├── modules/            # 13 independent modules, each with its own CMakeLists.txt
│   ├── platform/
│   │   ├── windows/        # Win32/COM wallpaper + OS-hooks backend
│   │   └── linux/          # DE-detection + argv-based wallpaper backend
│   └── app/                # weatherpaperd entry point (main.cpp)
├── tests/                  # one doctest suite per testable module
├── assets/default_theme/   # bundled offline default theme (placeholder art)
├── third_party/            # vendored doctest + nlohmann/json (header-only)
├── ARCHITECTURE.md
├── CONTRIBUTING.md
├── USERGUIDE.md
├── DEVELOPER_GUIDE.md
└── CMakeLists.txt
```

regardless.


./build/src/app/weatherpaperd 
