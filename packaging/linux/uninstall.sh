#!/usr/bin/env bash
set -euo pipefail

# WeatherPaper Linux Uninstaller

echo "=== WeatherPaper Uninstaller ==="

# 1. Stop running daemon
pkill -TERM -f weatherpaperd 2>/dev/null || true

# 2. Check if installed via dpkg
if which dpkg >/dev/null 2>&1 && dpkg -l weatherpaper >/dev/null 2>&1; then
    echo "Uninstalling Debian package (requires sudo)..."
    sudo dpkg -r weatherpaper
    echo "Debian package uninstalled."
fi

# 3. Clean user-mode files
PREFIX="${HOME}/.local"
rm -f "${PREFIX}/bin/weatherpaperd"
rm -rf "${PREFIX}/share/weatherpaper"
rm -f "${PREFIX}/share/applications/weatherpaper.desktop"
rm -f "${PREFIX}/share/icons/hicolor/32x32/apps/weatherpaper.png"
rm -f "${HOME}/.config/autostart/weatherpaper.desktop"

# Refresh desktop menu database
if which update-desktop-database >/dev/null 2>&1; then
    update-desktop-database "${PREFIX}/share/applications" || true
fi

echo "WeatherPaper has been uninstalled."
