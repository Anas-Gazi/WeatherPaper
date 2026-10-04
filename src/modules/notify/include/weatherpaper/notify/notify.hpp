// weatherpaper/notify/notify.hpp
//
// Module: notify (Section 3.13)
// Layer:  thin interface + platform-conditional implementation. Sourced
// from the same WeatherSnapshot the wallpaper pipeline already uses
// (weather_fetch::WeatherSnapshot::is_severe) - notify never fetches
// weather itself, it only decides whether/how to surface an
// already-fetched severe-weather flag as a native OS toast.
//
// NON-BLOCKING GUARANTEE (Section 3.13: "must never block or delay the
// wallpaper-update pipeline if disabled or if it fails"): every method
// here is designed to be called fire-and-forget from the app's main
// orchestration loop AFTER the wallpaper has already been resolved and
// set - never awaited before proceeding, and every implementation must
// swallow its own errors internally rather than propagating them, so a
// broken/missing notification backend (e.g. no notification daemon
// running on a minimal Linux setup) can never stall or fail the wallpaper
// pipeline itself.
#pragma once

#include <memory>
#include <string>

#include "weatherpaper/wallpaper_engine_core/wallpaper_engine_core.hpp"

namespace weatherpaper::notify {

struct SevereWeatherAlert {
    wallpaper_engine_core::Condition condition;
    double temperature_c = 0.0;
    std::string location_display_name; // human-readable only - SECURITY
                                         // (Section 5): never pass raw
                                         // lat/lon coordinates into a
                                         // notification body or log line.
};

// Pure formatting helper (fully unit-testable, no OS dependency): produces
// the title/body text for a given alert. Kept separate from the actual OS
// toast call so the wording can be tested without a display server.
struct NotificationText {
    std::string title;
    std::string body;
};
[[nodiscard]] NotificationText format_severe_weather_notification(const SevereWeatherAlert& alert);

class INotifier {
public:
    virtual ~INotifier() = default;
    // Fire-and-forget: implementations must not throw and should treat any
    // internal failure (no notification daemon, permission denied, etc.)
    // as a silent no-op - see file header "NON-BLOCKING GUARANTEE".
    virtual void show(const NotificationText& text) noexcept = 0;
};

// Toggle-able, off by default (Section 3.13) - this is enforced by
// config::AppConfig::severe_weather_notifications_enabled defaulting to
// false; this module itself has no opinion on the toggle and always sends
// when asked to, so the app orchestration layer is the single place that
// decides whether to call send_if_enabled() at all.
class NullNotifier : public INotifier {
public:
    void show(const NotificationText&) noexcept override {} // used when disabled or on unsupported platforms
};

#if defined(_WIN32)
// Windows: native toast via the Shell_NotifyIcon "balloon" API, or
// WinRT ToastNotificationManager on Windows 10/11 for a modern Action
// Center toast. VERIFICATION STATUS: same caveat as platform_windows -
// written against the documented API, not compiled/run on real Windows
// hardware in this drop (see docs/ARCHITECTURE.md).
class WindowsToastNotifier : public INotifier {
public:
    void show(const NotificationText& text) noexcept override;
};
#elif defined(__linux__)
// Linux: org.freedesktop.Notifications via the `notify-send` CLI (present
// on essentially every desktop Linux distribution with a notification
// daemon running - GNOME/KDE/XFCE/Cinnamon/MATE all ship one). Invoked via
// argv-only exec (no shell), consistent with Section 5's anti-injection
// requirement applied throughout the rest of the codebase - see
// src/modules/notify/src/notify_linux.cpp.
class LinuxNotifySendNotifier : public INotifier {
public:
    void show(const NotificationText& text) noexcept override;
};
#endif

// Convenience factory: returns the real platform notifier, or NullNotifier
// on any platform without one implemented yet.
[[nodiscard]] std::unique_ptr<INotifier> create_platform_notifier();

} // namespace weatherpaper::notify
