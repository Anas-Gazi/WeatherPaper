#!/usr/bin/env bash
set -euo pipefail

# WeatherPaper Debian (.deb) release builder
# Builds optimized lightweight release package

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-deb"
DIST_DIR="${ROOT_DIR}/dist"

echo "=== WeatherPaper Release .deb Package Builder ==="
echo "Project Root: ${ROOT_DIR}"
echo "Building in:   ${BUILD_DIR}"

mkdir -p "${DIST_DIR}"

# 1. Configure Release build with optimizations enabled
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DWEATHERPAPER_BUILD_SETTINGS_UI=ON \
    -DWEATHERPAPER_BUILD_TRAY_UI=ON \
    -DWEATHERPAPER_BUILD_TESTS=OFF \
    -DWEATHERPAPER_WITH_CURL=ON \
    -DWEATHERPAPER_WITH_OPENSSL=ON

# 2. Build binaries
cmake --build "${BUILD_DIR}" -j"$(nproc)"

# 3. Strip symbols to ensure minimal size (< 5MB binary)
if which strip >/dev/null 2>&1; then
    echo "Stripping symbols from binary for optimal size..."
    strip "${BUILD_DIR}/src/app/weatherpaperd"
fi

# 4. Generate Debian package using CPack
cd "${BUILD_DIR}"
cpack -G DEB

# 5. Move generated package to dist/
cp -f weatherpaper*.deb "${DIST_DIR}/" || true

echo "=== Build Complete! ==="
echo "Generated packages in ${DIST_DIR}:"
ls -lh "${DIST_DIR}"/*.deb
