# WeatherPaper — User Guide

WeatherPaper changes your desktop wallpaper to match the weather and time
of day where you live. This guide covers installation and day-to-day use. If you're a
developer looking to build or extend the app, see
[`CONTRIBUTING.md`](CONTRIBUTING.md) and [`ARCHITECTURE.md`](ARCHITECTURE.md)
instead.

## Installation & Setup

WeatherPaper is designed to be extremely lightweight, taking virtually **0% CPU** at idle, **< 40 MB of RAM**, and **0 GPU resources** for static wallpapers.

### Windows 10 & Windows 11

1. **Download the Installer:**
   - Download `WeatherPaper-Setup-v1.0.0.exe` from the latest GitHub Release (or compile using `packaging/windows/build-windows.bat`).
2. **Install:**
   - Double-click the installer and follow the setup wizard.
   - You can choose to automatically start WeatherPaper when Windows boots.
   - A shortcut will be placed in your Start Menu and system tray.
3. **Portable Mode (Alternative):**
   - Download `WeatherPaper-Windows-Portable.zip`.
   - Extract anywhere, and double-click `packaging/windows/install-portable.bat` to register autostart.

### Linux (All Distributions / Ubuntu / Debian / Fedora / Arch)

For direct download links, see the **[Installation Guide (INSTALL.md)](INSTALL.md)**.

1. **⚡ One-Click Installer (`weatherpaper-installer.run`) — Easiest:**
   - Download [`weatherpaper-installer.run`](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/weatherpaper-installer.run).
   - In your file manager, right-click → **Properties** → **Permissions** → check **"Allow executing file as program"**, then double-click.
   - Or run from terminal:
     ```bash
     chmod +x ~/Downloads/weatherpaper-installer.run
     ~/Downloads/weatherpaper-installer.run
     ```
   - Automatically installs the program, default themes, desktop launcher, and autostart on login (no `sudo` required).

2. **Debian Package (`.deb`):**
   - Download [`weatherpaper-0.1.0-Linux.deb`](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/weatherpaper-0.1.0-Linux.deb).
   - Install via terminal:
     ```bash
     sudo dpkg -i weatherpaper-0.1.0-Linux.deb
     sudo apt-get install -f   # ensures any missing runtime libs are installed
     ```
   - Or right-click the `.deb` file in your file manager and select **Open With Software Install**.

3. **Universal Linux Installer (Source / Non-root):**
   - If using Arch, Fedora, openSUSE, or installing from repository:
     ```bash
     ./packaging/linux/install.sh
     ```

---

## What you'll see

Once running, WeatherPaper sits quietly in your system tray. Every 15–30
minutes (configurable) it checks the weather for your location and, if the
condition or time-of-day has changed enough to warrant it, crossfades to a
new wallpaper that matches — for example, a rainy evening scene, or a
clear night sky.

If you're offline, it keeps showing the last wallpaper it successfully
resolved from cached weather data. If you've *never* been online (or the
cache is unreadable), it falls back to a bundled default appropriate for
the season.

## The tray icon

Right-click (or left-click, depending on your desktop) the tray icon for:

- **Pause / Resume auto-updates** — temporarily stop WeatherPaper from
  checking the weather or changing your wallpaper, without quitting it.
- **Refresh Now** — force an immediate weather check and wallpaper update,
  instead of waiting for the next scheduled poll.
- **Open Settings...** — opens the settings window (see below).
- **Open Gallery...** — jumps straight to the Gallery tab of the settings
  window, where your own wallpapers live.
- **Quit WeatherPaper**

## The settings window

The settings window has five sections, listed down the left side:

### General

- **Location** — auto-detected by default; toggle off to enter one
  manually if auto-detection isn't available on your system yet.
- **Units** — Celsius or Fahrenheit, used for temperature display and for
  the "extreme heat/cold" severe-weather threshold.
- **Weather update interval** — how often WeatherPaper checks the weather,
  from 15 to 30 minutes. Lower values are more responsive but use slightly
  more network/battery.

### Themes

Browse and manage installed theme packs. Each theme pack is a themed
collection of wallpapers, already tagged for you by weather and time of
day — install one and it's immediately usable. Uninstalling a pack removes
its images and its tags, but never anything from your own Gallery.

### Gallery

Your own wallpapers. You can add images and videos in two easy ways:

1. **Drag and drop:** Drag image or video files directly from your file manager (GNOME Files/Nautilus, Dolphin, Explorer) anywhere onto the Gallery page or the designated Drop Zone.
2. **Add File button:** Click **Add File...** to select files from a file browser.

- **Supported formats:** **PNG, JPG, JPEG, WebP** for static wallpapers; **MP4, WebM** for video wallpapers.
- **Smart Automatic Tagging:** When you drop or add a file, WeatherPaper automatically inspects the filename for keywords (e.g. `night`, `rain`, `clear`, `sun`, `snow`, `fog`, `cloud`) and pre-selects the appropriate tags for you!

#### Tagging and Sizing

After adding a file, select it in the gallery list to edit its weather conditions, solar times of day (e.g. "rain" + "night"), and fit mode:

| Fit mode | What it does |
|---|---|
| **Fill** | Crops to fill the screen with no distortion — may cut off edges. Good default for photos. |
| **Fit** | Shows the whole image, adding bars on the sides or top/bottom if it doesn't match your screen's shape. |
| **Stretch** | Forces the image to exactly fill the screen — may distort proportions. |
| **Center** | Shows the image at its original size, centered, with no resizing at all. |
| **Tile** | Repeats the image at its original size to cover the screen — useful for small pattern images. |

#### ⚡ Instant Wallpaper Refresh (No Restart Required!)
Whenever you:
- Check or uncheck a condition or time-of-day tag
- Change the fit mode (e.g. Stretch, Fill, Fit)
- Delete or add a wallpaper
- Or click the **"🔄 Refresh Wallpaper"** button in the gallery toolbar

WeatherPaper immediately updates your desktop background in less than **1 second** — you never need to close and restart the application!

### Performance

- **Enable animated/video wallpapers** — off by default. Turning this on
  lets video/looping wallpapers play instead of just static images, at the
  cost of higher CPU/GPU and battery use. Recommended only on machines
  with a dedicated or reasonably capable integrated GPU.
- **Pause animated wallpaper when...** — screen is locked / a fullscreen
  app or game is active / battery saver is on. All on by default, so
  animated wallpapers never compete with your game's frame rate or drain
  your battery unnecessarily.

### About / Updates

Shows the currently installed versions of the core engine, the settings
UI, and the asset catalog — these update independently of each other, so
you might get a new theme pack without a full app update, or vice versa.
**Check for Updates** looks for newer versions of any of the three.

## Severe weather alerts

Off by default. If enabled, WeatherPaper will show a system notification
for storms or extreme temperatures, sourced from the same weather check
that drives your wallpaper — it never makes an extra network call just for
this.

## Troubleshooting

**My wallpaper never changes.** Check that at least one theme pack (the
bundled default counts) is installed, and that your system clock/timezone
is correct — sunrise/sunset calculations depend on it.

**Wallpaper doesn't fill the whole screen the way I expect.** Check the
fit mode for that specific image in the Gallery tab — it's set per-image,
not globally.

**On Linux, nothing happens at all.** WeatherPaper needs to recognize your
desktop environment to know how to set the wallpaper. It supports GNOME,
KDE Plasma, XFCE, Cinnamon, MATE, Hyprland, and Sway out of the box, with a
generic fallback for other X11 window managers (via `feh`, if installed).
If none of these match your setup, please file an issue — see
`CONTRIBUTING.md` for how to help add support for your desktop environment.

**Video wallpaper doesn't appear even though I enabled it.** This is a
known current limitation — see `ARCHITECTURE.md`'s verification-status
notes. Static wallpapers are fully supported; video wallpaper rendering is
still being finished.
