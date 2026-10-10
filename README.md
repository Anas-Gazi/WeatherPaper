# WeatherPaper

A cross-platform, modular, live weather-reactive wallpaper engine for **Windows 10/11** and **Linux** (GNOME, KDE Plasma, XFCE, Cinnamon, MATE, Hyprland, and Sway).

WeatherPaper adapts your desktop wallpaper to live weather conditions and real sunrise/sunset-based time of day. It also supports a general-purpose wallpaper workflow using tagged images and video clips.

Built in C++20 with a modular architecture, event-driven updates, and an offline-capable default theme.

## Features

- **Weather-reactive wallpapers:** Switches wallpapers based on conditions such as sunny, cloudy, rain, snow, storm, fog, and clear.
- **Sunrise/sunset-aware time of day:** Determines morning, day, evening, and night using actual sunrise and sunset data.
- **Static and video wallpapers:** Supports images and optional video wallpapers on capable hardware.
- **Tag-based matching:** Matches wallpapers to weather conditions and time-of-day tags.
- **Theme packs:** Supports downloadable theme packs with integrity and signature verification.
- **Offline fallback:** Caches weather data and falls back to bundled assets when live data is unavailable.
- **Efficient updates:** Uses event-driven and timer-based processing rather than continuous busy polling.
- **Cross-platform architecture:** Provides Windows and Linux platform backends.

Weather data is provided by [Open-Meteo](https://open-meteo.com), which does not require an API key for its standard weather API.

## Project status

WeatherPaper is actively developed. The project includes a modular C++20 codebase and unit tests for its core components.

The Windows and Linux implementations have different levels of platform-specific verification. See [ARCHITECTURE.md](ARCHITECTURE.md) and [CONTRIBUTING.md](CONTRIBUTING.md) for implementation details, verification status, and development instructions.

## Download and installation

Download prebuilt packages from the **[GitHub Releases page](https://github.com/Anas-Gazi/WeatherPaper/releases)**.

### Windows 10 and Windows 11

Two package formats are intended to be available:

| Package | Best for | Installation |
|---|---|---|
| **Windows Setup (`.exe`)** | Users who want a conventional installer | Run the setup program and follow the wizard. |
| **Portable ZIP (`.zip`)** | Users who prefer extracting the application to a chosen folder | Extract the archive and run `WeatherPaper.exe`. |

**Windows Setup**

Download `WeatherPaper-Setup-v1.0.0.exe` from the release assets, run it, and follow the setup wizard. The installer can create shortcuts and offers optional startup configuration.

**Portable ZIP**

Download `WeatherPaper-v1.0.0-windows-x64.zip`, extract it to a folder, and run `WeatherPaper.exe`.

To configure automatic startup for the portable version, run `install-portable.bat` from the extracted folder. This changes your current Windows user's startup configuration; it does not install the application system-wide.

For complete instructions, troubleshooting, and removal steps, see [INSTALL.md](INSTALL.md).

> The download links above require the corresponding files to be uploaded to GitHub Releases. If a file is not attached to the release yet, use the Releases page to check which packages are currently available.

### Linux

See [INSTALL.md](INSTALL.md) for Linux installation options, including Debian packages and the universal installer.

## Build from source

### Prerequisites

On Ubuntu or Debian-based distributions:

```bash
sudo apt-get update
sudo apt-get install cmake build-essential libssl-dev libcurl4-openssl-dev qt6-base-dev libayatana-appindicator3-dev
```

### Configure and build

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DWEATHERPAPER_BUILD_SETTINGS_UI=ON \
  -DWEATHERPAPER_BUILD_TRAY_UI=ON

cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
```

Run the application in one-shot mode:

```bash
./build/src/app/weatherpaperd --once
```

For Windows-specific build and packaging instructions, see [CONTRIBUTING.md](CONTRIBUTING.md) and the scripts under `packaging/windows/`.

## Documentation

| Document | Purpose |
|---|---|
| [INSTALL.md](INSTALL.md) | Download, install, and uninstall instructions |
| [USERGUIDE.md](USERGUIDE.md) | End-user configuration and wallpaper management |
| [ARCHITECTURE.md](ARCHITECTURE.md) | System architecture and design |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Build instructions and contribution guidelines |
| [DEVELOPER_GUIDE.md](DEVELOPER_GUIDE.md) | Quick reference for developers |

## Repository layout

```text
WeatherPaper/
├── assets/
│   └── default_theme/       # Bundled default wallpaper theme
├── src/
│   ├── modules/              # Reusable application modules
│   ├── platform/
│   │   ├── windows/          # Windows platform backend
│   │   └── linux/            # Linux platform backend
│   └── app/                  # Application entry point and launcher target
├── packaging/
│   ├── windows/              # Windows installer and packaging scripts
│   └── linux/                # Linux packaging and installation scripts
├── tests/                    # Unit tests
├── third_party/              # Vendored dependencies
├── CMakeLists.txt
├── INSTALL.md
├── USERGUIDE.md
├── ARCHITECTURE.md
└── CONTRIBUTING.md
```

## License

WeatherPaper is licensed under the [GNU General Public License v3.0](LICENSE).
