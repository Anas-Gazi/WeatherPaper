// weatherpaper/tray_ui/tray_ui.hpp
//
// Module: tray_ui (Section 3.11)
// Layer:  thin interface + platform-native implementation. Section 3.11 is
// explicit that this must use "the OS's native tray API (Win32
// Shell_NotifyIcon on Windows; AppIndicator/StatusNotifierItem on Linux),
// not a custom-drawn tray icon" - so, like platform_windows/platform_linux,
// there is no cross-platform GUI toolkit here, just two native backends
// behind one interface.
#pragma once

#include <functional>
#include <memory>
#include <string>

namespace weatherpaper::tray_ui {

// The five actions Section 3.11 requires: pause/resume auto-updates,
// force-refresh-now, open settings, open gallery, quit. Modeled as
// callbacks the app orchestration layer supplies, rather than tray_ui
// calling back into weather_fetch/settings_ui/updater directly - keeping
// this module's only dependency the C++ standard library, consistent with
// "clean, minimal public interface" (Section 2.1).
struct TrayCallbacks {
    std::function<void()> on_toggle_pause_resume; // pause/resume auto-updates
    std::function<void()> on_force_refresh_now;
    std::function<void()> on_open_settings;
    std::function<void()> on_open_gallery;
    std::function<void()> on_quit;
};

class ITrayIcon {
public:
    virtual ~ITrayIcon() = default;

    // Creates and shows the tray icon with the given menu callbacks.
    // `icon_path` points at a bundled .ico (Windows) / .png (Linux) asset.
    virtual bool create(const TrayCallbacks& callbacks, const std::string& icon_path) = 0;

    // Updates the tooltip/menu label reflecting pause state (e.g. "Pause
    // auto-updates" <-> "Resume auto-updates") - called by the app layer
    // whenever the user (or a menu click) changes that state.
    virtual void set_paused_label(bool is_paused) = 0;

    // Must be pumped periodically from the app's existing event loop
    // (same "no dedicated spin thread" contract as
    // platform_common::IPlatformHooks::pump_events) so the native menu can
    // dispatch clicks - no-op on backends that are fully event-driven via
    // their own OS callback registration.
    virtual void pump_events() = 0;

    virtual void destroy() = 0;
};

[[nodiscard]] std::unique_ptr<ITrayIcon> create_platform_tray_icon();

} // namespace weatherpaper::tray_ui
