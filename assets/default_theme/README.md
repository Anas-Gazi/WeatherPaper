# Bundled Default Theme — `assets/default_theme/`

This is WeatherPaper's built-in, offline-available theme pack (Section 3.1/4
of the spec: "Bundled default theme pack, fully tagged, available offline
out of the box").

## About the images

The 13 images in `images/` are **placeholder solid-color JPEGs**, generated
with `ffmpeg -f lavfi -i color=c=<hex>:s=640x360`, not photographic or
illustrated artwork. They exist so the tagging, resolution, fallback-chain,
and platform wallpaper-setting pipeline is genuinely end-to-end testable and
runnable today, without depending on licensed or AI-generated art assets
being available in this environment.

**Before shipping a real release**, replace every file under `images/` with
actual photography or illustration and keep `tags.json`'s `id`/`file`/`type`
fields pointed at the new filenames — the tag/fit_mode metadata format
doesn't need to change.

## Coverage

| id | tags | purpose |
|---|---|---|
| `morning_default` / `day_default` / `evening_default` / `night_default` | one time-of-day tag each | guarantee the Resolver's time-only fallback always finds *something*, for every time bucket, regardless of weather |
| `clear_day`, `clear_night`, `sunny_day`, `rain_day`, `rain_night`, `snow_day`, `storm_night`, `fog_morning`, `cloudy_evening` | exact `condition + time` pairs | richer, exact matches for the most common states |

Every other `condition + time` combination not listed above still resolves
correctly via `wallpaper_engine_core::Resolver`'s fallback chain (exact →
time-only → condition-only) — see that module's header comment.

## Files

- `tags.json` — the tag_system-format asset index (loaded directly by
  `src/app/main.cpp` at startup and merged into the live `TagIndex`).
- `pack.json` — this pack's own identity/version metadata, versioned
  independently of the core engine and UI (Section 2.5).
- `images/*.jpg` — the placeholder art described above.
