#include "weatherpaper/config/config.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>

#if defined(_WIN32)
  #include <windows.h>
  #include <shlobj.h>
#endif

namespace weatherpaper::config {

using json = nlohmann::json;

namespace {

const char* unit_to_string(TemperatureUnit u) {
    return u == TemperatureUnit::Fahrenheit ? "fahrenheit" : "celsius";
}
TemperatureUnit unit_from_string(const std::string& s) {
    return s == "fahrenheit" ? TemperatureUnit::Fahrenheit : TemperatureUnit::Celsius;
}

const char* policy_to_string(tag_system::SelectionPolicy p) {
    switch (p) {
        case tag_system::SelectionPolicy::Random:  return "random";
        case tag_system::SelectionPolicy::Pinned:  return "pinned";
        case tag_system::SelectionPolicy::Sequential:
        default:                                    return "sequential";
    }
}
tag_system::SelectionPolicy policy_from_string(const std::string& s) {
    if (s == "random") return tag_system::SelectionPolicy::Random;
    if (s == "pinned") return tag_system::SelectionPolicy::Pinned;
    return tag_system::SelectionPolicy::Sequential;
}

} // namespace

AppConfig AppConfig::load_from_file(const std::string& path, bool* out_ok) {
    AppConfig cfg; // defaults
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        if (out_ok) *out_ok = true; // first run, not an error
        return cfg;
    }
    json j;
    try {
        in >> j;
    } catch (const std::exception&) {
        if (out_ok) *out_ok = false; // corrupt config, defaults used instead
        return cfg;
    }

    // Every field below uses json::value() with a default, so an OLDER
    // config file (missing newer fields after a core update) or a config
    // file with extra/unknown fields (from a NEWER version, if the user
    // downgrades) both load without error - forward/backward tolerance is
    // required by the "independently versioned, updatable" design goal
    // (Section 2.5).
    if (j.contains("location") && j["location"].is_object()) {
        const auto& loc = j["location"];
        cfg.location.auto_detected = loc.value("auto_detected", true);
        cfg.location.latitude = loc.value("latitude", 0.0);
        cfg.location.longitude = loc.value("longitude", 0.0);
        cfg.location.display_name = loc.value("display_name", std::string{});
    }
    cfg.units = unit_from_string(j.value("units", std::string("celsius")));
    cfg.weather_poll_interval = std::chrono::minutes(j.value("weather_poll_interval_minutes", 20));
    if (j.contains("active_theme_pack_ids") && j["active_theme_pack_ids"].is_array()) {
        for (const auto& id : j["active_theme_pack_ids"]) {
            if (id.is_string()) cfg.active_theme_pack_ids.push_back(id.get<std::string>());
        }
    }
    if (j.contains("performance") && j["performance"].is_object()) {
        const auto& p = j["performance"];
        cfg.performance.animated_wallpaper_enabled = p.value("animated_wallpaper_enabled", false);
        cfg.performance.pause_on_battery_saver = p.value("pause_on_battery_saver", true);
        cfg.performance.pause_when_fullscreen_app_active = p.value("pause_when_fullscreen_app_active", true);
        cfg.performance.pause_when_locked = p.value("pause_when_locked", true);
    }
    cfg.selection_policy = policy_from_string(j.value("selection_policy", std::string("sequential")));
    cfg.severe_weather_notifications_enabled = j.value("severe_weather_notifications_enabled", false);
    cfg.auto_update_enabled = j.value("auto_update_enabled", true);
    if (j.contains("monitors") && j["monitors"].is_array()) {
        for (const auto& m : j["monitors"]) {
            MonitorTarget mt;
            mt.monitor_id = m.value("monitor_id", std::string{});
            mt.use_same_as_primary = m.value("use_same_as_primary", true);
            cfg.monitors.push_back(std::move(mt));
        }
    }

    if (out_ok) *out_ok = true;
    return cfg;
}

bool AppConfig::save_to_file(const std::string& path) const {
    json j;
    j["schema_version"] = 1; // config's own on-disk schema version, independent
                              // of core/assets/ui semver (Section 2.5)
    j["location"] = {
        {"auto_detected", location.auto_detected},
        {"latitude", location.latitude},
        {"longitude", location.longitude},
        {"display_name", location.display_name},
    };
    j["units"] = unit_to_string(units);
    j["weather_poll_interval_minutes"] = weather_poll_interval.count();
    j["active_theme_pack_ids"] = active_theme_pack_ids;
    j["performance"] = {
        {"animated_wallpaper_enabled", performance.animated_wallpaper_enabled},
        {"pause_on_battery_saver", performance.pause_on_battery_saver},
        {"pause_when_fullscreen_app_active", performance.pause_when_fullscreen_app_active},
        {"pause_when_locked", performance.pause_when_locked},
    };
    j["selection_policy"] = policy_to_string(selection_policy);
    j["severe_weather_notifications_enabled"] = severe_weather_notifications_enabled;
    j["auto_update_enabled"] = auto_update_enabled;
    j["monitors"] = json::array();
    for (const auto& m : monitors) {
        j["monitors"].push_back({
            {"monitor_id", m.monitor_id},
            {"use_same_as_primary", m.use_same_as_primary},
        });
    }

    // Atomic write, same rationale as tag_system::TagIndex::save_to_file.
    const std::string tmp_path = path + ".tmp";
    {
        std::ofstream out(tmp_path, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) return false;
        out << j.dump(2);
        if (!out.good()) return false;
    }
    if (std::rename(tmp_path.c_str(), path.c_str()) != 0) {
        std::remove(tmp_path.c_str());
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------
// Path resolution
// ---------------------------------------------------------------------
#if defined(_WIN32)

namespace {
std::string known_folder_local_appdata() {
    PWSTR wpath = nullptr;
    std::string result;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &wpath))) {
        // Narrow conversion is fine here: WeatherPaper's own install path
        // never contains non-ASCII by construction, and this is only used
        // to build our OWN subdirectory name ("WeatherPaper"), not to
        // round-trip arbitrary user paths.
        int len = WideCharToMultiByte(CP_UTF8, 0, wpath, -1, nullptr, 0, nullptr, nullptr);
        std::string buf(len, '\0');
        WideCharToMultiByte(CP_UTF8, 0, wpath, -1, buf.data(), len, nullptr, nullptr);
        if (!buf.empty() && buf.back() == '\0') buf.pop_back();
        result = buf;
        CoTaskMemFree(wpath);
    }
    return result;
}
} // namespace

std::string default_config_dir() { return known_folder_local_appdata() + "\\WeatherPaper\\config"; }
std::string default_cache_dir()  { return known_folder_local_appdata() + "\\WeatherPaper\\cache"; }
std::string default_data_dir()   { return known_folder_local_appdata() + "\\WeatherPaper\\data"; }
std::string default_log_dir()    { return known_folder_local_appdata() + "\\WeatherPaper\\logs"; }

#else // Linux / other POSIX

namespace {
std::string home_dir() {
    if (const char* h = std::getenv("HOME"); h != nullptr && *h != '\0') return h;
    return "/tmp"; // ASSUMPTION: extremely defensive last resort if $HOME
                    // is somehow unset (e.g. minimal container); avoids
                    // building a path off a null pointer.
}
std::string xdg_or_default(const char* env_var, const std::string& fallback_suffix) {
    if (const char* v = std::getenv(env_var); v != nullptr && *v != '\0') return v;
    return home_dir() + fallback_suffix;
}
} // namespace

// Per Section 3.9: "respect XDG Base Directory spec - config in
// ~/.config/weatherpaper, cached data in ~/.cache/weatherpaper".
std::string default_config_dir() { return xdg_or_default("XDG_CONFIG_HOME", "/.config") + "/weatherpaper"; }
std::string default_cache_dir()  { return xdg_or_default("XDG_CACHE_HOME", "/.cache") + "/weatherpaper"; }
std::string default_data_dir()   { return xdg_or_default("XDG_DATA_HOME", "/.local/share") + "/weatherpaper"; }
std::string default_log_dir()    { return default_data_dir() + "/logs"; }

#endif

} // namespace weatherpaper::config
