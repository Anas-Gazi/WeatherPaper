#!/usr/bin/env bash
set -euo pipefail

# WeatherPaper Self-Extracting One-Click Installer Builder
# Generates 'weatherpaper-installer.run' which packages the binary, assets,
# icons, desktop launcher, and autostart configuration into a single clickable file.

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
DIST_DIR="${ROOT_DIR}/dist"
TMP_STAGE="$(mktemp -d /tmp/wp-installer-stage-XXXXXX)"

cleanup() {
    rm -rf "${TMP_STAGE}"
}
trap cleanup EXIT

echo "=== Building WeatherPaper Self-Extracting Installer ==="
mkdir -p "${DIST_DIR}"

# 1. Ensure binary is built and stripped
BUILD_DIR="${ROOT_DIR}/build-deb"
if [ ! -f "${BUILD_DIR}/src/app/weatherpaperd" ]; then
    echo "Building Release binary..."
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DWEATHERPAPER_BUILD_SETTINGS_UI=ON \
        -DWEATHERPAPER_BUILD_TRAY_UI=ON \
        -DWEATHERPAPER_BUILD_TESTS=OFF
    cmake --build "${BUILD_DIR}" -j"$(nproc)"
fi

mkdir -p "${TMP_STAGE}/payload/bin"
mkdir -p "${TMP_STAGE}/payload/share/weatherpaper/default_theme"
mkdir -p "${TMP_STAGE}/payload/share/weatherpaper/icons"
mkdir -p "${TMP_STAGE}/payload/share/applications"
mkdir -p "${TMP_STAGE}/payload/share/icons/hicolor/32x32/apps"
mkdir -p "${TMP_STAGE}/payload/autostart"

cp -f "${BUILD_DIR}/src/app/weatherpaperd" "${TMP_STAGE}/payload/bin/weatherpaperd"
strip --strip-unneeded "${TMP_STAGE}/payload/bin/weatherpaperd" || true
chmod +x "${TMP_STAGE}/payload/bin/weatherpaperd"

cp -rf "${ROOT_DIR}/assets/default_theme/"* "${TMP_STAGE}/payload/share/weatherpaper/default_theme/"
cp -rf "${ROOT_DIR}/assets/icons/"* "${TMP_STAGE}/payload/share/weatherpaper/icons/"
cp -f "${ROOT_DIR}/assets/icons/weatherpaper-icon-32.png" "${TMP_STAGE}/payload/share/icons/hicolor/32x32/apps/weatherpaper.png"
cp -f "${ROOT_DIR}/packaging/linux/weatherpaper.desktop" "${TMP_STAGE}/payload/share/applications/weatherpaper.desktop"
cp -f "${ROOT_DIR}/packaging/linux/weatherpaper-autostart.desktop" "${TMP_STAGE}/payload/autostart/weatherpaper-autostart.desktop"

# Create tarball of payload
tar -czf "${TMP_STAGE}/payload.tar.gz" -C "${TMP_STAGE}/payload" .

# 2. Generate the self-extracting header script
INSTALLER_OUTPUT="${DIST_DIR}/weatherpaper-installer.run"

cat > "${TMP_STAGE}/header.sh" << 'EOF'
#!/usr/bin/env bash
set -e

# ========================================================
# WeatherPaper One-Click Installer
# ========================================================

notify_gui() {
    local title="$1"
    local message="$2"
    if which zenity >/dev/null 2>&1; then
        zenity --info --title="${title}" --text="${message}" --width=350 2>/dev/null || true
    elif which notify-send >/dev/null 2>&1; then
        notify-send "${title}" "${message}" 2>/dev/null || true
    fi
}

echo "=========================================="
echo "   Installing WeatherPaper..."
echo "=========================================="

PREFIX="${HOME}/.local"
BIN_DIR="${PREFIX}/bin"
SHARE_DIR="${PREFIX}/share"
AUTOSTART_DIR="${HOME}/.config/autostart"

mkdir -p "${BIN_DIR}"
mkdir -p "${SHARE_DIR}/weatherpaper"
mkdir -p "${SHARE_DIR}/applications"
mkdir -p "${SHARE_DIR}/icons/hicolor/32x32/apps"
mkdir -p "${AUTOSTART_DIR}"

TMP_EXTRACT="$(mktemp -d /tmp/wp-install-XXXXXX)"
cleanup() {
    rm -rf "${TMP_EXTRACT}"
}
trap cleanup EXIT

# Extract embedded tar.gz payload from this script
ARCHIVE_START_LINE=$(awk '/^__PAYLOAD_BELOW__/ {print NR + 1; exit 0; }' "$0")
tail -n +"${ARCHIVE_START_LINE}" "$0" | tar -xz -C "${TMP_EXTRACT}"

# Install files
echo "Installing files to ${PREFIX}..."
cp -f "${TMP_EXTRACT}/bin/weatherpaperd" "${BIN_DIR}/weatherpaperd"
chmod +x "${BIN_DIR}/weatherpaperd"

cp -rf "${TMP_EXTRACT}/share/weatherpaper/"* "${SHARE_DIR}/weatherpaper/"
cp -rf "${TMP_EXTRACT}/share/icons/"* "${SHARE_DIR}/icons/"

# Generate correct desktop file pointing to installed binary
cat > "${SHARE_DIR}/applications/weatherpaper.desktop" << DESK
[Desktop Entry]
Version=1.0
Type=Application
Name=WeatherPaper
Comment=Live weather-reactive dynamic wallpaper engine
Exec=${BIN_DIR}/weatherpaperd
Icon=weatherpaper
Terminal=false
Categories=Utility;Settings;DesktopSettings;
StartupNotify=true
Actions=Gallery;Settings;Refresh;

[Desktop Action Gallery]
Name=Open Gallery
Exec=${BIN_DIR}/weatherpaperd --gallery

[Desktop Action Settings]
Name=Open Settings
Exec=${BIN_DIR}/weatherpaperd --settings

[Desktop Action Refresh]
Name=Update Wallpaper Now
Exec=${BIN_DIR}/weatherpaperd --refresh
DESK

cp -f "${SHARE_DIR}/applications/weatherpaper.desktop" "${AUTOSTART_DIR}/weatherpaper-autostart.desktop"
chmod +x "${SHARE_DIR}/applications/weatherpaper.desktop"

# Refresh desktop menus
if which update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${SHARE_DIR}/applications" 2>/dev/null || true
fi
if which gtk-update-icon-cache >/dev/null 2>&1; then
    gtk-update-icon-cache -f -t "${SHARE_DIR}/icons/hicolor" 2>/dev/null || true
fi

echo "=========================================="
echo " WeatherPaper installed successfully!"
echo " Location:  ${BIN_DIR}/weatherpaperd"
echo " Autostart: Enabled"
echo "=========================================="

# Start daemon in background if not already running
if ! pgrep -x "weatherpaperd" >/dev/null 2>&1; then
    echo "Starting WeatherPaper in background..."
    nohup "${BIN_DIR}/weatherpaperd" >/dev/null 2>&1 &
fi

notify_gui "WeatherPaper" "WeatherPaper has been installed successfully!\nIt is now running in your system tray."

exit 0

__PAYLOAD_BELOW__
EOF

# Combine header and payload into final .run installer
cat "${TMP_STAGE}/header.sh" "${TMP_STAGE}/payload.tar.gz" > "${INSTALLER_OUTPUT}"
chmod +x "${INSTALLER_OUTPUT}"

# Also create a .sh named copy for convenience
cp -f "${INSTALLER_OUTPUT}" "${DIST_DIR}/weatherpaper-installer.sh"

echo "=== Generated Installer Files ==="
ls -lh "${DIST_DIR}/weatherpaper-installer.run"
ls -lh "${DIST_DIR}/weatherpaper-installer.sh"
echo "Done!"
