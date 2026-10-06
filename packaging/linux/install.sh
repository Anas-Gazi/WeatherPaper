#!/usr/bin/env bash
set -euo pipefail

# WeatherPaper Universal Linux Installer
# Automatically installs WeatherPaper, desktop entries, icons, and autostart.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Check if pre-built .deb package exists in dist/ or build/
DEB_FILE=$(find "${ROOT_DIR}" -name "weatherpaper*.deb" 2>/dev/null | head -n 1 || true)

if [ -n "${DEB_FILE}" ] && which dpkg >/dev/null 2>&1; then
    echo "Found pre-built Debian package: ${DEB_FILE}"
    echo "Installing via dpkg (requires sudo)..."
    sudo dpkg -i "${DEB_FILE}" || sudo apt-get install -f -y
    echo "Installed successfully!"
    exit 0
fi

# Fallback: Source install to ~/.local (no root required)
PREFIX="${HOME}/.local"
BIN_DIR="${PREFIX}/bin"
SHARE_DIR="${PREFIX}/share"
AUTOSTART_DIR="${HOME}/.config/autostart"

echo "=== WeatherPaper User-Mode Installer ==="
echo "Target Prefix: ${PREFIX}"

mkdir -p "${BIN_DIR}"
mkdir -p "${SHARE_DIR}/weatherpaper/default_theme"
mkdir -p "${SHARE_DIR}/weatherpaper/icons"
mkdir -p "${SHARE_DIR}/applications"
mkdir -p "${SHARE_DIR}/icons/hicolor/32x32/apps"
mkdir -p "${AUTOSTART_DIR}"

# Locate or build binary
BUILD_BIN="${ROOT_DIR}/build/src/app/weatherpaperd"
if [ ! -f "${BUILD_BIN}" ]; then
    echo "Building WeatherPaper binary..."
    cmake -S "${ROOT_DIR}" -B "${ROOT_DIR}/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DWEATHERPAPER_BUILD_SETTINGS_UI=ON \
        -DWEATHERPAPER_BUILD_TRAY_UI=ON \
        -DWEATHERPAPER_BUILD_TESTS=OFF
    cmake --build "${ROOT_DIR}/build" -j"$(nproc)"
fi

cp -f "${BUILD_BIN}" "${BIN_DIR}/weatherpaperd"
chmod +x "${BIN_DIR}/weatherpaperd"

# Copy assets and default theme
cp -rf "${ROOT_DIR}/assets/default_theme/"* "${SHARE_DIR}/weatherpaper/default_theme/"
cp -rf "${ROOT_DIR}/assets/icons/"* "${SHARE_DIR}/weatherpaper/icons/"
cp -f "${ROOT_DIR}/assets/icons/weatherpaper-icon-32.png" "${SHARE_DIR}/icons/hicolor/32x32/apps/weatherpaper.png"

# Setup Desktop Launcher and Autostart
cat > "${SHARE_DIR}/applications/weatherpaper.desktop" <<EOF
[Desktop Entry]
Version=1.0
Type=Application
Name=WeatherPaper
Comment=Live weather-reactive wallpaper engine
Exec=${BIN_DIR}/weatherpaperd
Icon=weatherpaper
Terminal=false
Categories=Utility;Settings;
StartupNotify=true
EOF

cp -f "${SHARE_DIR}/applications/weatherpaper.desktop" "${AUTOSTART_DIR}/weatherpaper.desktop"

# Refresh desktop menu database if available
if which update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${SHARE_DIR}/applications" || true
fi

echo ""
echo "=== Installation Successful! ==="
echo "WeatherPaper has been installed to ${BIN_DIR}/weatherpaperd"
echo "It has been configured to automatically start on login."
echo "You can launch it now by running: ${BIN_DIR}/weatherpaperd"
