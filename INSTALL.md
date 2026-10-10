# WeatherPaper — Download and Installation Guide

This guide explains how to download, install, run, and uninstall WeatherPaper on Windows 10/11 and Linux.

## Official downloads

**[Open WeatherPaper Releases on GitHub](https://github.com/Anas-Gazi/WeatherPaper/releases)**

Download packages only from the official repository's Releases page.

### Windows packages

| Package | Description |
|---|---|
| `WeatherPaper-Setup-v1.0.0.exe` | Graphical Windows installer |
| `WeatherPaper-v1.0.0-windows-x64.zip` | Portable application archive |

These filenames must match the actual release assets. If either package is not listed on the release page, it has not been published there under that filename.

---

## Windows 10 and Windows 11

### Method A — Windows Setup installer

This is the recommended option for users who want a conventional installation.

1. Open the [GitHub Releases page](https://github.com/Anas-Gazi/WeatherPaper/releases).
2. Download `WeatherPaper-Setup-v1.0.0.exe` from the appropriate release.
3. Open the downloaded installer.
4. If Windows displays a security warning, verify that you downloaded the file from the official repository before deciding whether to proceed.
5. Follow the setup wizard and choose the installation options you want.
6. After installation, launch WeatherPaper from the Start menu or an available shortcut.

The installer is designed to install the application files locally. Depending on the options you select, it can also create a desktop shortcut and configure automatic startup when you sign in to Windows.

### Method B — Portable ZIP

Choose this option if you prefer to keep WeatherPaper in a folder of your choice.

1. Open the [GitHub Releases page](https://github.com/Anas-Gazi/WeatherPaper/releases).
2. Download `WeatherPaper-v1.0.0-windows-x64.zip`.
3. Right-click the ZIP file and select **Extract All...**.
4. Extract it to a folder where you want to keep WeatherPaper.
5. Open the extracted folder.
6. Double-click `WeatherPaper.exe` to launch the application.

Keep the extracted files together. The application needs its bundled runtime dependencies, plugins, and theme assets.

#### Optional: Configure automatic startup

To configure WeatherPaper to start automatically when you sign in to Windows:

1. Open the extracted WeatherPaper folder.
2. Run `install-portable.bat`.
3. Follow the prompts displayed by the script.

The script configures startup for the current Windows user. It does not install WeatherPaper system-wide.

To disable portable automatic startup, run `uninstall-portable.bat` if that script is included in your downloaded package. Otherwise, remove the WeatherPaper entry from the following Windows Registry location:

`HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Run`

Remove only the value named `WeatherPaper`.

---

## Uninstalling WeatherPaper on Windows

### If you used the setup installer

1. Open **Settings**.
2. Navigate to **Apps → Installed apps**.
3. Find **WeatherPaper**.
4. Select the uninstall option and follow the prompts.

### If you used the portable ZIP

1. Disable automatic startup first, if you enabled it.
2. Close WeatherPaper.
3. Delete the folder where you extracted the application.

Deleting the portable folder alone may not remove an automatic-startup entry. Remove that entry before deleting the folder.

---

## Linux installation

Linux packages and installation options are published through the [GitHub Releases page](https://github.com/Anas-Gazi/WeatherPaper/releases), when available.

### Debian, Ubuntu, and compatible distributions

If a Debian package is attached to the release, download it and install it using:

```bash
sudo dpkg -i weatherpaper-0.1.0-Linux.deb
sudo apt-get install -f
```

Replace the filename with the exact package name shown in the release assets if it differs.

### Universal installer

If `weatherpaper-installer.run` is available in the release assets, download it and run:

```bash
chmod +x weatherpaper-installer.run
./weatherpaper-installer.run
```

Review the script and its requested permissions before running installers downloaded from the internet.

### Running WeatherPaper

Depending on the package and desktop environment, launch WeatherPaper from the application menu or use the installed executable where supported.

### Uninstalling on Debian-based systems

If WeatherPaper was installed as a Debian package:

```bash
sudo apt remove weatherpaper
```

Use the uninstallation method appropriate to the package you installed.

---

## Troubleshooting

### The application does not start on Windows

- Make sure you extracted the entire portable ZIP rather than running the executable from inside the archive.
- Keep all DLLs, Qt plugin directories, and bundled assets in their original relative locations.
- Download the package again if files are missing or extraction failed.
- Check the repository's [Issues page](https://github.com/Anas-Gazi/WeatherPaper/issues) for known problems.

### Weather does not update

WeatherPaper uses the Open-Meteo service for weather data. Check your internet connection and verify that the application can reach the service. Cached data and the bundled default theme may be used when live data is unavailable.

### Automatic startup does not work

Verify that the startup option was enabled and that the application still exists at the configured path. If you move or delete the portable folder after enabling startup, update or remove the startup entry.

---

## More information

- [README.md](README.md) — Project overview and features
- [USERGUIDE.md](USERGUIDE.md) — Using and configuring WeatherPaper
- [GitHub Releases](https://github.com/Anas-Gazi/WeatherPaper/releases) — Published downloads
- [GitHub Issues](https://github.com/Anas-Gazi/WeatherPaper/issues) — Bug reports and support
