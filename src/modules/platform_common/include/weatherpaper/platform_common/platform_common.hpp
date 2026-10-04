// weatherpaper/platform_common/platform_common.hpp
//
// Module: platform_common (Sections 3.5 / 3.6)
// Layer:  interface only - pure virtual contracts, no implementation, no
//         platform headers included here. platform_windows and
//         platform_linux each provide a concrete implementation of every
//         interface in this file; render_engine, asset_manager, and the
//         app orchestration layer (src/app) code exclusively against these
//         interfaces and NEVER #include a Windows or Linux header directly.
//         That is the entire point of "upper layers are fully OS-agnostic"
//         (Section 3.6).
#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "weatherpaper/scaling_and_fit/scaling_and_fit.hpp"
#include "weatherpaper/tag_system/tag_system.hpp"

namespace weatherpaper::platform_common {

// --- Monitor enumeration (Section 3.4: per-monitor wallpaper support) ----
struct MonitorInfo {
    std::string id;      // stable OS-reported identifier (device path / name)
    std::string friendly_name;
    int x = 0, y = 0;     // top-left, in the OS's virtual-desktop coordinate space
    int width = 0, height = 0;
    bool is_primary = false;
};

// --- Wallpaper-setting ----------------------------------------------------

struct WallpaperRequest {
    std::string file_path;
    scaling_and_fit::FitMode fit_mode = scaling_and_fit::FitMode::Fill;
    tag_system::AssetType asset_type = tag_system::AssetType::Image;
    // Empty = apply to every monitor (Section 3.4 graceful-degradation
    // default for DEs without per-monitor support). Non-empty = apply only
    // to the monitor with this MonitorInfo::id (Windows and some Linux DEs
    // support this; others silently apply to all monitors instead - see
    // each backend's doc comment for exactly which).
    std::string target_monitor_id;
};

enum class SetWallpaperResult {
    Success,
    Failed,          // backend command/API call itself failed
    UnsupportedOnThisDesktop // e.g. per-monitor requested on a DE that can't
};

class IPlatformWallpaper {
public:
    virtual ~IPlatformWallpaper() = default;

    // Applies a *static image* wallpaper. For video/animated wallpapers,
    // render_engine does NOT call this - see render_engine.hpp for why
    // (video compositing bypasses the OS wallpaper API entirely).
    virtual SetWallpaperResult set_static_wallpaper(const WallpaperRequest& request) = 0;

    virtual std::vector<MonitorInfo> enumerate_monitors() const = 0;

    // True if this backend can set a different image per monitor. When
    // false, set_static_wallpaper() ignores target_monitor_id and applies
    // to every monitor identically (Section 3.4's documented graceful
    // degradation for Linux DEs that don't support per-monitor wallpapers).
    virtual bool supports_per_monitor_wallpaper() const = 0;

    // Human-readable identifier of which concrete backend is active, e.g.
    // "windows-desktopwallpaper-api", "linux-gnome-gsettings",
    // "linux-kde-plasma-dbus", "linux-wayland-swww", "linux-x11-feh". Used
    // for the diagnostic log line Section 3.6 explicitly asks for:
    // "Log clearly ... which backend was detected and used".
    virtual std::string backend_name() const = 0;
};

// --- OS hooks (Section 3.5: idle/wake, power state, fullscreen, network) -

enum class PowerState {
    OnACPower,
    OnBatteryNormal,
    OnBatterySaver // Windows "Battery Saver" / Linux power-profiles-daemon "power-saver"
};

class IPlatformHooks {
public:
    virtual ~IPlatformHooks() = default;

    using VoidCallback = std::function<void()>;
    using PowerStateCallback = std::function<void(PowerState)>;

    // Fires once when the system enters/leaves idle (screensaver/lock) and
    // wake, respectively - used to pause/resume animated wallpaper
    // rendering (Section 3.4).
    virtual void on_session_locked(VoidCallback cb) = 0;
    virtual void on_session_unlocked(VoidCallback cb) = 0;

    virtual void on_power_state_changed(PowerStateCallback cb) = 0;
    [[nodiscard]] virtual PowerState current_power_state() const = 0;

    // Polling accessor rather than only an event - fullscreen-app state is
    // cheap to query on both OSes and render_engine's render loop already
    // ticks every frame it needs to, so a poll avoids extra hook plumbing.
    [[nodiscard]] virtual bool is_fullscreen_app_active() const = 0;

    // Fires when the OS reports the network came back up (Section 3.1:
    // "retry immediately on OS network-reconnect events").
    virtual void on_network_reconnected(VoidCallback cb) = 0;

    // Must be called periodically (e.g. once per second) from the app's
    // main/event loop so hook implementations that rely on polling
    // (notably some Linux D-Bus signal dispatch) get a chance to run.
    // No-op on platforms whose hooks are fully event-driven, but has to
    // exist here as it's part of the "no busy-polling ourselves" contract:
    // callers pump this from an existing OS timer/event loop rather than a
    // dedicated spin thread (Section 2.2).
    virtual void pump_events() = 0;
};

// --- Factories (defined in platform_windows.cpp / platform_linux.cpp; only
// one of the two translation units is ever compiled into a given binary -
// see each module's CMakeLists.txt) -------------------------------------
[[nodiscard]] std::unique_ptr<IPlatformWallpaper> create_platform_wallpaper();
[[nodiscard]] std::unique_ptr<IPlatformHooks> create_platform_hooks();

} // namespace weatherpaper::platform_common
