#!/usr/bin/env bash
# Regenerates the placeholder solid-color JPEGs under
# assets/default_theme/images/ using ffmpeg's lavfi color source.
#
# These are NOT meant to be final art (see assets/default_theme/README.md)
# - this script exists so a contributor can quickly regenerate/extend the
# placeholder set (e.g. to add a new tag combination) without needing any
# image-editing tool, and so the provenance of the current placeholder
# images is fully reproducible/auditable.
#
# Usage: ./scripts/generate_placeholder_theme_images.sh
# Requires: ffmpeg on PATH.

set -euo pipefail

OUT_DIR="$(dirname "$0")/../assets/default_theme/images"
mkdir -p "$OUT_DIR"

gen() {
    local name="$1" color="$2"
    ffmpeg -y -f lavfi -i "color=c=${color}:s=640x360" -frames:v 1 -q:v 3 \
        "${OUT_DIR}/${name}.jpg" -loglevel error
    echo "generated ${name}.jpg (${color})"
}

# Time-of-day-only fallbacks (guarantee every bucket has *something*)
gen morning_default  "0xFFC896"
gen day_default      "0x87CEEB"
gen evening_default  "0xC87850"
gen night_default    "0x0A0A28"

# Exact condition+time combinations
gen clear_day        "0x64B4FF"
gen clear_night      "0x05053C"
gen sunny_day        "0xFFDC50"
gen rain_day         "0x5A6E82"
gen rain_night       "0x1E2337"
gen snow_day         "0xE6EBF0"
gen storm_night      "0x140A1E"
gen fog_morning      "0xC8C8C8"
gen cloudy_evening   "0x96786E"

echo "Done. Remember to keep assets/default_theme/tags.json's 'file' entries"
echo "in sync with any filenames you add or rename here."
