# WeatherPaper — Download & Installation Guide

This guide contains the official download links and step-by-step instructions for installing and running **WeatherPaper** on **Linux** and **Windows 10 / 11**.

---

## 🔗 Official Download Links

All releases and installer binaries are published on GitHub Releases:

👉 **Releases Page:** [https://github.com/Anas-Gazi/WeatherPaper/releases/tag/v0.1.0](https://github.com/Anas-Gazi/WeatherPaper/releases/tag/v0.1.0)

### Direct Download Links (v0.1.0)

| Platform | Package Type | Direct Download Link | Description |
| :--- | :--- | :--- | :--- |
| **Linux (All Distros)** | **⚡ One-Click Installer** | [**Download `weatherpaper-installer.run`**](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/weatherpaper-installer.run) | **Single clickable file.** Installs all files, shortcuts, icons, and autostart automatically! |
| **Linux (Ubuntu / Debian / Mint)** | `.deb` Package | [Download `weatherpaper-0.1.0-Linux.deb`](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/weatherpaper-0.1.0-Linux.deb) | Standard Debian package for `dpkg`/`apt`. |
| **Linux (Any Distro - Standalone)** | `.tar.gz` Archive | [Download `weatherpaper-0.1.0-Linux.tar.gz`](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/weatherpaper-0.1.0-Linux.tar.gz) | Portable tarball archive. |
| **Windows 10 & 11** | **⚡ 1-Click Installer** | [**Download `WeatherPaper-Setup.exe`**](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/WeatherPaper-Setup.exe) | **One click.** Download, run, and WeatherPaper installs everything automatically! |

---

## 🐧 Linux Installation

### Method A: ⚡ One-Click Installer (`weatherpaper-installer.run`) — Easiest

1. **Download:** [**`weatherpaper-installer.run`**](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/weatherpaper-installer.run)
2. **Run it:**
   - Either **Right-click** → **Properties** → **Permissions** → check **"Allow executing file as program"**, then **Double-Click** it.
   - Or run in terminal:
     ```bash
     chmod +x ~/Downloads/weatherpaper-installer.run
     ~/Downloads/weatherpaper-installer.run
     ```
3. **Done!** The installer automatically:
   - Extracts and installs the program and all required assets and themes.
   - Creates the application shortcut in your desktop app launcher with the WeatherPaper logo.
   - Configures automatic startup when your PC boots or logs in.
   - Launches WeatherPaper in the background immediately with a success notification!

### Method B: Debian Package (`.deb`)


1. Open a terminal and navigate to the directory where you downloaded the `.deb` file:
   ```bash
   cd ~/Downloads
   ```

2. Install the package using `dpkg`:
   ```bash
   sudo dpkg -i weatherpaper-0.1.0-Linux.deb
   sudo apt-get install -f
   ```
   *(Running `sudo apt-get install -f` automatically resolves and installs any required dependencies like Qt runtime libraries).*

### Method B: Graphical Interface (GUI)

1. Open your file manager and navigate to your **Downloads** folder.
2. Double-click on `weatherpaper-0.1.0-Linux.deb`.
3. Your system's Software Center or package installer will appear. Click **Install** and enter your password.

### Running WeatherPaper on Linux

- **Application Menu:** Press the `Super` (Windows) key, search for **WeatherPaper**, and click the icon.
- **Terminal:** You can launch the daemon directly anytime by typing:
  ```bash
  weatherpaperd
  ```
- **System Tray:** A weather indicator icon will appear in your top bar / system tray. Right-click it to open the Settings, view the Gallery, or manually force a wallpaper refresh.

### Uninstalling from Linux

```bash
sudo apt remove weatherpaper
```

---

## 🪟 Windows 10 & 11 Installation

### Method A: ⚡ One-Click Installer — Easiest

1. **Download:** [**`WeatherPaper-Setup.exe`**](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/WeatherPaper-Setup.exe)
2. **Run it:** Double-click `WeatherPaper-Setup.exe`. Windows may show a SmartScreen prompt — click **More info** → **Run anyway**.
3. **Done!** The installer automatically downloads and sets up all files, creates a desktop shortcut, and configures WeatherPaper to start with Windows.

### Method B: Portable ZIP (No Installation)

1. **Download:** [`WeatherPaper-v1.0.0-windows-x64.zip`](https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/WeatherPaper-v1.0.0-windows-x64.zip)
2. **Extract:** Right-click → **Extract All...** to a folder of your choice.
3. **Run:** Double-click `install-portable.bat` inside the extracted folder to launch and configure autostart.


### Uninstalling from Windows

- Open **Windows Settings** → **Apps** → **Installed apps**.
- Find **WeatherPaper** in the list, click the three dots (`...`), and select **Uninstall**.

---

## 💡 Quick Tips & Features

- **Drag and Drop Wallpapers:** Open the **Gallery** tab and drag your favorite images or videos directly into the drop zone.
- **Instant Refresh:** When you tag an image (e.g. `Night`, `Rain`, `Sunny`) or click **🔄 Refresh Wallpaper**, your desktop wallpaper updates immediately without restarting the program.
- **Ultra Lightweight:** Uses ~38 MB RAM and 0% CPU when idle.
