// WeatherPaper application entry point.
//
// This is the ONLY file in the codebase that knows about every module at
// once - by design (Section 2.1: modular architecture). Its job is purely
// orchestration: load config, load the tag index, poll weather, resolve a
// wallpaper, hand it to the platform layer, and sleep without busy-polling
// (Section 2.2) until the next tick or a relevant OS event.
//
// Supports `--once` for a single pipeline iteration (fetch -> resolve ->
// set -> exit) instead of the normal long-running loop - primarily useful
// for CI/sandbox smoke-testing the wiring without a real display session.
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <thread>

#if defined(WEATHERPAPER_BUILD_SETTINGS_UI)
#include <QApplication>
#include <QIcon>
#include <QPixmap>
#include <QListWidget>
#endif

#if defined(WEATHERPAPER_BUILD_SETTINGS_UI)
#include "weatherpaper/settings_ui/settings_ui.hpp"
#endif
#if defined(WEATHERPAPER_BUILD_TRAY_UI)
#include "weatherpaper/tray_ui/tray_ui.hpp"
#endif
#include "weatherpaper/asset_manager/asset_manager.hpp"
#include "weatherpaper/config/config.hpp"
#include "weatherpaper/notify/notify.hpp"
#include "weatherpaper/platform_common/platform_common.hpp"
#include "weatherpaper/render_engine/render_engine.hpp"
#include "weatherpaper/tag_system/tag_system.hpp"
#include "weatherpaper/time_of_day/time_of_day.hpp"
#include "weatherpaper/wallpaper_engine_core/wallpaper_engine_core.hpp"
#include "weatherpaper/weather_fetch/weather_fetch.hpp"

#ifndef WEATHERPAPER_BUNDLED_ASSETS_DIR
#define WEATHERPAPER_BUNDLED_ASSETS_DIR "./assets/default_theme"
#endif

namespace wp = weatherpaper;

namespace {

std::mutex g_shutdown_mutex;
std::condition_variable g_shutdown_cv;
volatile std::sig_atomic_t g_shutdown_requested = 0;

void handle_signal(int) {
    g_shutdown_requested = 1;
    g_shutdown_cv.notify_all();
}

struct AppPaths {
    std::string config_file;
    std::string tag_index_file;
    std::string weather_cache_file;
    std::string bundled_tags_file;
};

AppPaths resolve_paths() {
    AppPaths paths;
    paths.config_file = wp::config::default_config_dir() + "/config.json";
    paths.tag_index_file = wp::config::default_data_dir() + "/tag_index.json";
    paths.weather_cache_file = wp::config::default_cache_dir() + "/weather_cache.json";
    paths.bundled_tags_file = std::string(WEATHERPAPER_BUNDLED_ASSETS_DIR) + "/tags.json";
    return paths;
}

bool run_pipeline_once(wp::config::AppConfig& cfg,
                         wp::tag_system::TagIndex& tag_index,
                         wp::weather_fetch::IHttpClient& http_client,
                         wp::platform_common::IPlatformWallpaper& wallpaper_platform,
                         wp::notify::INotifier& notifier,
                         const std::string& weather_cache_path,
                         int& consecutive_failures,
                         wp::wallpaper_engine_core::CrossfadeScheduler& crossfade) {
    using namespace wp;

    weather_fetch::OpenMeteoProvider provider;
    std::optional<weather_fetch::WeatherSnapshot> snapshot;

    if (auto live = provider.fetch(http_client, cfg.location.latitude, cfg.location.longitude)) {
        snapshot = live;
        consecutive_failures = 0;
        weather_fetch::write_cache(weather_cache_path, *live);
    } else {
        consecutive_failures++;
        std::cerr << "[weatherpaper] live weather fetch failed (attempt " << consecutive_failures
                  << "), falling back to cache\n";
        snapshot = weather_fetch::read_cache(weather_cache_path);
    }

    time_of_day::Bucket bucket;
    wallpaper_engine_core::Condition condition;
    auto now = std::chrono::system_clock::now();

    if (snapshot.has_value() && snapshot->is_valid()) {
        time_of_day::SunTimes sun{snapshot->sunrise, snapshot->sunset};
        bucket = time_of_day::resolve(sun, now);
        condition = snapshot->condition;
    } else {
        std::cerr << "[weatherpaper] no live weather and no cache - using offline/seasonal fallback\n";
        bucket = time_of_day::Bucket::Night;
        condition = wallpaper_engine_core::Condition::Clear;
    }

    wallpaper_engine_core::Resolver resolver(cfg.selection_policy);
    auto resolved = resolver.resolve(tag_index, condition, bucket);
    if (!resolved.has_value()) {
        std::cerr << "[weatherpaper] no matching wallpaper found for condition="
                  << wallpaper_engine_core::to_string(condition)
                  << " time=" << time_of_day::to_string(bucket)
                  << " - check that a theme pack is installed\n";
        return false;
    }

    std::cout << "[weatherpaper] resolved asset_id=" << resolved->asset_id
              << " file=" << resolved->file_path
              << " fit_mode=" << scaling_and_fit::to_string(resolved->fit_mode)
              << (resolved->used_fallback ? " (fallback match)" : " (exact match)") << "\n";

    crossfade.set_target(resolved->asset_id, std::chrono::steady_clock::now());

    platform_common::WallpaperRequest request;
    request.file_path = resolved->file_path;
    request.fit_mode = resolved->fit_mode;
    request.asset_type = resolved->type;
    auto set_result = wallpaper_platform.set_static_wallpaper(request);
    if (set_result != platform_common::SetWallpaperResult::Success) {
        std::cerr << "[weatherpaper] platform backend (" << wallpaper_platform.backend_name()
                  << ") failed to set wallpaper\n";
    } else {
        std::cout << "[weatherpaper] wallpaper set via backend=" << wallpaper_platform.backend_name() << "\n";
    }

    if (cfg.severe_weather_notifications_enabled && snapshot.has_value() && snapshot->is_severe) {
        notify::SevereWeatherAlert alert{condition, snapshot->temperature_c, cfg.location.display_name};
        notifier.show(notify::format_severe_weather_notification(alert));
    }

    return set_result == platform_common::SetWallpaperResult::Success;
}

} // namespace

int main(int argc, char** argv) {
    bool once = false;
    std::string screenshot_file;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--once") once = true;
        if (std::string(argv[i]) == "--screenshot" && i + 1 < argc) {
            screenshot_file = argv[++i];
        }
    }

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    auto paths = resolve_paths();

    bool cfg_ok = false;
    auto cfg = wp::config::AppConfig::load_from_file(paths.config_file, &cfg_ok);
    if (!cfg_ok) {
        std::cerr << "[weatherpaper] warning: config file was malformed, using defaults\n";
    }

    if (cfg.location.latitude == 0.0 && cfg.location.longitude == 0.0 &&
        cfg.location.display_name.empty()) {
        cfg.location.latitude = 23.8103;
        cfg.location.longitude = 90.4125;
        cfg.location.display_name = "Dhaka, Bangladesh (placeholder default)";
        std::cerr << "[weatherpaper] no location configured - using placeholder default\n";
    }

    wp::tag_system::TagIndex tag_index;
    bool bundled_ok = false;
    auto bundled = wp::tag_system::TagIndex::load_from_file(paths.bundled_tags_file, &bundled_ok);
    for (const auto& rec : bundled.list_all()) {
        auto copy = rec;
        copy.file_path = std::string(WEATHERPAPER_BUNDLED_ASSETS_DIR) + "/" + rec.file_path;
        copy.source_pack_id = "bundled-default";
        tag_index.upsert(copy);
    }
    if (!bundled_ok || bundled.list_all().empty()) {
        std::cerr << "[weatherpaper] warning: bundled default theme not found/empty at "
                  << paths.bundled_tags_file << "\n";
    }
    bool user_index_ok = false;
    auto user_index = wp::tag_system::TagIndex::load_from_file(paths.tag_index_file, &user_index_ok);
    for (const auto& rec : user_index.list_all()) tag_index.upsert(rec);

    std::cout << "[weatherpaper] loaded " << tag_index.list_all().size() << " tagged assets\n";

#if defined(WEATHERPAPER_WITH_CURL)
    wp::weather_fetch::CurlHttpClient http_client;
#else
    struct NullHttpClient : wp::weather_fetch::IHttpClient {
        wp::weather_fetch::HttpResponse get(const std::string&) override { return {true, 0, ""}; }
    } http_client;
#endif

    auto wallpaper_platform = wp::platform_common::create_platform_wallpaper();
    auto hooks = wp::platform_common::create_platform_hooks();
    auto notifier = wp::notify::create_platform_notifier();

    wp::asset_manager::ThemePackManager pack_manager(wp::config::default_data_dir());

    if (wallpaper_platform) {
        std::cout << "[weatherpaper] platform wallpaper backend: "
                  << wallpaper_platform->backend_name() << "\n";
    }

    // --once: single pipeline run, no UI - used for CI/smoke-tests.
    if (once) {
        wp::wallpaper_engine_core::CrossfadeScheduler crossfade;
        int consecutive_failures = 0;
        run_pipeline_once(cfg, tag_index, http_client, *wallpaper_platform, *notifier,
                          paths.weather_cache_file, consecutive_failures, crossfade);
        return 0;
    }

#if defined(WEATHERPAPER_BUILD_SETTINGS_UI)
    if (!screenshot_file.empty()) {
        QApplication qt_app(argc, argv);
        wp::settings_ui::MainWindow settings_window(&cfg, &tag_index, &pack_manager);
        settings_window.show();
        qt_app.processEvents();
        auto* sidebar = settings_window.findChild<QListWidget*>("sidebar");
        std::vector<std::string> tab_names = {"general", "themes", "gallery", "performance", "about"};
        if (sidebar) {
            for (int r = 0; r < sidebar->count() && r < static_cast<int>(tab_names.size()); ++r) {
                sidebar->setCurrentRow(r);
                qt_app.processEvents();
                std::string path = screenshot_file + "_" + tab_names[r] + ".png";
                settings_window.grab().save(QString::fromStdString(path));
            }
        }
        std::cout << "[weatherpaper] all 5 tab screenshots saved using prefix: " << screenshot_file << "\n";
        return 0;
    }
#endif

    // -------------------------------------------------------------------------
    // Full GUI mode.
    // Qt requires the main thread for event dispatch. The weather poll loop
    // runs on a background thread so main() can call QApplication::exec().
    // -------------------------------------------------------------------------

    wp::wallpaper_engine_core::CrossfadeScheduler crossfade;
    int consecutive_failures = 0;

    // First pipeline run before showing the window so the desktop is already
    // updated when the settings window appears.
    run_pipeline_once(cfg, tag_index, http_client, *wallpaper_platform, *notifier,
                      paths.weather_cache_file, consecutive_failures, crossfade);

    // Background weather poll thread.
    std::thread poll_thread([&]() {
        std::cout << "[weatherpaper] poll thread started (interval: "
                  << cfg.weather_poll_interval.count() << " min)\n";
        while (!g_shutdown_requested) {
            auto interval = wp::weather_fetch::next_backoff_interval(
                cfg.weather_poll_interval, consecutive_failures);
            auto deadline = std::chrono::steady_clock::now() + interval;
            while (std::chrono::steady_clock::now() < deadline && !g_shutdown_requested) {
                std::unique_lock<std::mutex> lock(g_shutdown_mutex);
                g_shutdown_cv.wait_for(lock, std::chrono::seconds(1));
                if (hooks) hooks->pump_events();
            }
            if (g_shutdown_requested) break;
            run_pipeline_once(cfg, tag_index, http_client, *wallpaper_platform, *notifier,
                              paths.weather_cache_file, consecutive_failures, crossfade);
        }
        std::cout << "[weatherpaper] poll thread exiting\n";
    });

#if defined(WEATHERPAPER_BUILD_SETTINGS_UI)
    QApplication qt_app(argc, argv);
    qt_app.setApplicationName("WeatherPaper");
    qt_app.setApplicationVersion("1.0.0");
    qt_app.setWindowIcon(QIcon(QString::fromStdString(
        std::string(WEATHERPAPER_BUNDLED_ASSETS_DIR) + "/../../icons/weatherpaper-icon-32.png")));

    wp::settings_ui::MainWindow settings_window(&cfg, &tag_index, &pack_manager);

    // Wire instant wallpaper application callback from Gallery tab
    settings_window.set_apply_wallpaper_callback([&](const std::string& path,
                                                     wp::scaling_and_fit::FitMode fit,
                                                     wp::tag_system::AssetType type) {
        if (!wallpaper_platform) return;
        wp::platform_common::WallpaperRequest req;
        req.file_path = path;
        req.fit_mode = fit;
        req.asset_type = type;
        wallpaper_platform->set_static_wallpaper(req);
        std::cout << "[weatherpaper] applied wallpaper from gallery: " << path << "\n";
    });

    // Wire manual refresh trigger from Settings UI
    settings_window.set_force_refresh_callback([&]() {
        std::cout << "[weatherpaper] manual refresh requested from UI\n";
        g_shutdown_cv.notify_all();
    });

    settings_window.show();

#if defined(WEATHERPAPER_BUILD_TRAY_UI)
    auto tray_icon = wp::tray_ui::create_platform_tray_icon();
    wp::tray_ui::TrayCallbacks tray_callbacks;
    bool is_paused = false;

    tray_callbacks.on_toggle_pause_resume = [&]() {
        is_paused = !is_paused;
        if (tray_icon) tray_icon->set_paused_label(is_paused);
        if (!is_paused) g_shutdown_cv.notify_all();
    };
    tray_callbacks.on_force_refresh_now = [&]() {
        g_shutdown_cv.notify_all();
    };
    tray_callbacks.on_open_settings = [&]() {
        settings_window.show();
        settings_window.raise();
        settings_window.activateWindow();
    };
    tray_callbacks.on_open_gallery = [&]() {
        settings_window.select_tab(2); // Jump straight to Gallery tab
        settings_window.show();
        settings_window.raise();
        settings_window.activateWindow();
    };
    tray_callbacks.on_quit = [&]() {
        g_shutdown_requested = 1;
        g_shutdown_cv.notify_all();
        QApplication::quit();
    };

    const std::string tray_icon_path =
        std::string(WEATHERPAPER_BUNDLED_ASSETS_DIR) + "/../../icons/weatherpaper-icon-32.png";
    bool tray_ok = false;
    if (tray_icon && tray_icon->create(tray_callbacks, tray_icon_path)) {
        tray_ok = true;
    } else {
        std::cerr << "[weatherpaper] system tray unavailable (non-fatal)\n";
        tray_icon.reset();
    }
    // Only keep running on close if tray is active; otherwise closing window quits gracefully
    qt_app.setQuitOnLastWindowClosed(!tray_ok);
#else
    qt_app.setQuitOnLastWindowClosed(true);
#endif

    // Qt event loop owns the main thread from here.
    int exit_code = qt_app.exec();

    g_shutdown_requested = 1;
    g_shutdown_cv.notify_all();
    poll_thread.join();
    std::cout << "[weatherpaper] shutting down\n";
    return exit_code;

#else
    // Headless daemon mode - no Qt UI compiled.
    std::cout << "[weatherpaper] running headless (UI not built); Ctrl+C to quit\n";
    poll_thread.join();
    std::cout << "[weatherpaper] shutting down\n";
    return 0;
#endif
}
