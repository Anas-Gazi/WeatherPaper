// weatherpaper/config/config.hpp
//
// Module: config (Section 3.9)
// Layer:  pure logic + local file I/O (JSON on disk, XDG-aware paths).
//         No network. No wallpaper-setting OS calls (that's
//         platform_windows/platform_linux).
//
// Stores: location, units, update interval, active theme pack(s), monitor
// targeting, power-saving preferences, and tag-mapping overrides (e.g. a
// user's SelectionPolicy choice, and any pinned assets).
//
// SECURITY NOTE (Section 5): this struct intentionally keeps the user's
// precise lat/lon in memory and in the config file (needed to query the
// weather API), but callers (weather_fetch, notify, any logging call site)
// must NOT write Location::latitude/longitude into the log file - only a
// human-readable place name (Location::display_name) should ever be logged.
// See docs/ARCHITECTURE.md "Privacy & logging".
#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

#include "weatherpaper/tag_system/tag_system.hpp"

namespace weatherpaper::config {

enum class TemperatureUnit { Celsius, Fahrenheit };

struct Location {
    bool auto_detected = true;      // false once the user sets a manual override
    double latitude = 0.0;          // SECURITY: never log this - see file header
    double longitude = 0.0;         // SECURITY: never log this - see file header
    std::string display_name;       // e.g. "Dhaka, Bangladesh" - safe to log
};

// ASSUMPTION: modeled as a simple bool rather than an enum-with-more-tiers,
// because the spec only ever describes a binary choice: "static mode ...
// universal fallback ... let animated/video wallpaper be an opt-in higher
// tier" (Section 2.3). A future contributor wanting e.g. a mid-tier
// "animated but low-fps" mode can extend this struct without breaking the
// JSON schema (unknown-field-tolerant load, see config.cpp).
struct PerformancePrefs {
    bool animated_wallpaper_enabled = false; // opt-in (Section 2.3 / 4)
    bool pause_on_battery_saver = true;      // Section 3.4
    bool pause_when_fullscreen_app_active = true; // Section 3.4
    bool pause_when_locked = true;                // Section 3.4
};

struct MonitorTarget {
    std::string monitor_id;   // OS-reported monitor identifier
    bool use_same_as_primary = true; // if true, mirrors the primary monitor's
                                      // resolved wallpaper; if false, this
                                      // monitor can have independent tag
                                      // overrides in the future (Section 3.4
                                      // per-monitor support) - kept minimal
                                      // for v1 per YAGNI, but the schema
                                      // slot exists so it's forward-compatible.
};

struct AppConfig {
    Location location;
    TemperatureUnit units = TemperatureUnit::Celsius;
    std::chrono::minutes weather_poll_interval{20}; // within spec's 15-30min default range
    std::vector<std::string> active_theme_pack_ids;  // e.g. {"bundled-default"}
    PerformancePrefs performance;
    tag_system::SelectionPolicy selection_policy = tag_system::SelectionPolicy::Sequential;
    bool severe_weather_notifications_enabled = false; // "off by default" per Section 3.13
    bool auto_update_enabled = true;
    std::vector<MonitorTarget> monitors;

    // Loads from `path`. Missing file -> defaults (first run), matching
    // tag_system's "not found is not an error" convention. Malformed JSON
    // also degrades to defaults rather than crashing - a broken config file
    // must never prevent the app from launching (Section 2.5 spirit).
    [[nodiscard]] static AppConfig load_from_file(const std::string& path, bool* out_ok = nullptr);
    [[nodiscard]] bool save_to_file(const std::string& path) const;
};

// --- XDG / platform-appropriate default paths (Section 3.9) --------------
// These do not touch the filesystem (no mkdir) - callers create directories
// as needed. Implemented per-OS:
//   Windows -> %LOCALAPPDATA%\WeatherPaper\{config,cache,gallery,logs}
//   Linux   -> respects $XDG_CONFIG_HOME / $XDG_CACHE_HOME / $XDG_DATA_HOME
//              with the spec-mandated fallback defaults
//              (~/.config/weatherpaper, ~/.cache/weatherpaper,
//               ~/.local/share/weatherpaper) when those env vars are unset.
[[nodiscard]] std::string default_config_dir();
[[nodiscard]] std::string default_cache_dir();
[[nodiscard]] std::string default_data_dir();  // gallery, installed theme packs
[[nodiscard]] std::string default_log_dir();

} // namespace weatherpaper::config
