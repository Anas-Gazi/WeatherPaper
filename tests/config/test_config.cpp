#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "weatherpaper/config/config.hpp"

using namespace weatherpaper::config;

TEST_CASE("load_from_file on a missing path returns defaults, not an error") {
    bool ok = false;
    AppConfig cfg = AppConfig::load_from_file("/nonexistent/wp_config_missing.json", &ok);
    CHECK(ok);
    CHECK(cfg.units == TemperatureUnit::Celsius);
    CHECK(cfg.weather_poll_interval == std::chrono::minutes(20));
    CHECK(cfg.performance.animated_wallpaper_enabled == false);
    CHECK(cfg.severe_weather_notifications_enabled == false); // off by default, Section 3.13
}

TEST_CASE("save_to_file / load_from_file round-trips every field") {
    const std::string path = (std::filesystem::temp_directory_path() / "wp_config_roundtrip.json").string();
    std::remove(path.c_str());

    AppConfig cfg;
    cfg.location.auto_detected = false;
    cfg.location.latitude = 23.8103;
    cfg.location.longitude = 90.4125;
    cfg.location.display_name = "Dhaka, Bangladesh";
    cfg.units = TemperatureUnit::Fahrenheit;
    cfg.weather_poll_interval = std::chrono::minutes(15);
    cfg.active_theme_pack_ids = {"bundled-default", "sakura-pack"};
    cfg.performance.animated_wallpaper_enabled = true;
    cfg.performance.pause_on_battery_saver = false;
    cfg.selection_policy = weatherpaper::tag_system::SelectionPolicy::Random;
    cfg.severe_weather_notifications_enabled = true;
    cfg.auto_update_enabled = false;
    MonitorTarget mt;
    mt.monitor_id = "DISPLAY1";
    mt.use_same_as_primary = false;
    cfg.monitors.push_back(mt);

    REQUIRE(cfg.save_to_file(path));

    bool ok = false;
    AppConfig loaded = AppConfig::load_from_file(path, &ok);
    CHECK(ok);
    CHECK(loaded.location.auto_detected == false);
    CHECK(loaded.location.latitude == doctest::Approx(23.8103));
    CHECK(loaded.location.longitude == doctest::Approx(90.4125));
    CHECK(loaded.location.display_name == "Dhaka, Bangladesh");
    CHECK(loaded.units == TemperatureUnit::Fahrenheit);
    CHECK(loaded.weather_poll_interval == std::chrono::minutes(15));
    REQUIRE(loaded.active_theme_pack_ids.size() == 2);
    CHECK(loaded.active_theme_pack_ids[0] == "bundled-default");
    CHECK(loaded.performance.animated_wallpaper_enabled == true);
    CHECK(loaded.performance.pause_on_battery_saver == false);
    CHECK(loaded.selection_policy == weatherpaper::tag_system::SelectionPolicy::Random);
    CHECK(loaded.severe_weather_notifications_enabled == true);
    CHECK(loaded.auto_update_enabled == false);
    REQUIRE(loaded.monitors.size() == 1);
    CHECK(loaded.monitors[0].monitor_id == "DISPLAY1");
    CHECK(loaded.monitors[0].use_same_as_primary == false);

    std::remove(path.c_str());
}

TEST_CASE("load_from_file tolerates unknown/extra fields (forward compatibility)") {
    const std::string path = (std::filesystem::temp_directory_path() / "wp_config_future.json").string();
    {
        std::ofstream out(path);
        out << R"({
            "schema_version": 99,
            "units": "celsius",
            "some_future_field_this_binary_does_not_know_about": { "nested": true },
            "performance": { "animated_wallpaper_enabled": true, "brand_new_toggle": 42 }
        })";
    }
    bool ok = false;
    AppConfig cfg = AppConfig::load_from_file(path, &ok);
    CHECK(ok);
    CHECK(cfg.performance.animated_wallpaper_enabled == true);
    std::remove(path.c_str());
}

TEST_CASE("load_from_file on malformed JSON degrades to defaults rather than crashing") {
    const std::string path = (std::filesystem::temp_directory_path() / "wp_config_bad.json").string();
    {
        std::ofstream out(path);
        out << "{ not json at all !!";
    }
    bool ok = true;
    CHECK_NOTHROW(AppConfig::load_from_file(path, &ok));
    AppConfig cfg = AppConfig::load_from_file(path, &ok);
    CHECK_FALSE(ok);
    CHECK(cfg.units == TemperatureUnit::Celsius); // defaults
    std::remove(path.c_str());
}

#if !defined(_WIN32)
TEST_CASE("Linux default paths respect XDG_* env vars when set") {
    const char* old_config = std::getenv("XDG_CONFIG_HOME");
    std::string old_config_val = old_config ? old_config : "";
    bool had_old_config = old_config != nullptr;

    setenv("XDG_CONFIG_HOME", "/tmp/custom_xdg_config", 1);
    CHECK(default_config_dir() == "/tmp/custom_xdg_config/weatherpaper");

    if (had_old_config) setenv("XDG_CONFIG_HOME", old_config_val.c_str(), 1);
    else unsetenv("XDG_CONFIG_HOME");
}

TEST_CASE("Linux default paths fall back to ~/.config etc when XDG_* is unset") {
    const char* old_config = std::getenv("XDG_CONFIG_HOME");
    std::string old_config_val = old_config ? old_config : "";
    bool had_old_config = old_config != nullptr;
    unsetenv("XDG_CONFIG_HOME");

    std::string result = default_config_dir();
    CHECK(result.find(".config/weatherpaper") != std::string::npos);

    if (had_old_config) setenv("XDG_CONFIG_HOME", old_config_val.c_str(), 1);
}
#endif
