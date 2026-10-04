// Concrete Linux implementation of platform_common::IPlatformWallpaper.
// Wires together backend.hpp (DE detection + argv command building, fully
// unit tested) and process_runner.hpp (safe argv-based execution) - this
// file itself is thin glue and OS integration code, deliberately kept free
// of any decision logic so that logic stays in the unit-tested backend.cpp.
#include <cstdio>
#include <cstdlib>
#include <sstream>

#include "weatherpaper/platform_common/platform_common.hpp"
#include "weatherpaper/platform_linux/backend.hpp"
#include "weatherpaper/platform_linux/process_runner.hpp"

namespace weatherpaper::platform_linux {
namespace {

// Best-effort monitor enumeration via `xrandr --query` (X11 / XWayland).
// KNOWN LIMITATION: pure-Wayland compositors without XWayland have no
// universal equivalent (Hyprland exposes `hyprctl monitors`, Sway exposes
// `swaymsg -t get_outputs`, each with a different JSON shape) - a
// contributor wiring up true per-compositor multi-monitor support should
// add compositor-specific parsers here behind the same
// std::vector<MonitorInfo> return type (see CONTRIBUTING.md). Until then,
// this always returns at least one synthetic "primary" entry so upstream
// code never has to special-case "zero monitors".
std::vector<platform_common::MonitorInfo> enumerate_via_xrandr(IProcessRunner& runner) {
    std::vector<platform_common::MonitorInfo> monitors;
    // We don't have a "capture stdout" primitive in IProcessRunner (it's
    // deliberately minimal / injection-safe for the *setting* side) so
    // real stdout capture would need popen-with-argv (e.g. via a pipe +
    // fork, no shell) - left as a follow-up; for now we report a single
    // synthetic full-desktop monitor, which keeps set_static_wallpaper()
    // correct (applies to the whole desktop) even if per-monitor targeting
    // isn't available on this system yet.
    (void)runner;
    platform_common::MonitorInfo primary;
    primary.id = "primary";
    primary.friendly_name = "Primary Display";
    primary.x = 0;
    primary.y = 0;
    primary.width = 1920;
    primary.height = 1080;
    primary.is_primary = true;
    monitors.push_back(primary);
    return monitors;
}

class LinuxPlatformWallpaper : public platform_common::IPlatformWallpaper {
public:
    LinuxPlatformWallpaper() {
        RealEnvReader env;
        detected_de_ = detect_desktop_environment(env);
    }

    platform_common::SetWallpaperResult set_static_wallpaper(
        const platform_common::WallpaperRequest& request) override {

        if (request.asset_type == tag_system::AssetType::Video) {
            auto commands = build_video_command(detected_de_, request.file_path, request.fit_mode);
            if (commands.empty() || !commands.front().is_supported) {
                return platform_common::SetWallpaperResult::UnsupportedOnThisDesktop;
            }
            // Video wallpapers on Wayland (mpvpaper) are long-running
            // processes - launch detached and replace any previous
            // instance first (Section 3.6 / backend.cpp comments).
            runner_.terminate_by_program_name("mpvpaper");
            bool ok = true;
            for (const auto& cmd : commands) ok = runner_.run_detached(cmd) && ok;
            return ok ? platform_common::SetWallpaperResult::Success
                       : platform_common::SetWallpaperResult::Failed;
        }

        auto commands = build_static_image_command(detected_de_, request.file_path, request.fit_mode);
        if (commands.empty() || !commands.front().is_supported) {
            return platform_common::SetWallpaperResult::UnsupportedOnThisDesktop;
        }

        const bool is_wayland_bg = (detected_de_ == DesktopEnvironment::WaylandHyprland ||
                                     detected_de_ == DesktopEnvironment::WaylandSway);
        if (is_wayland_bg) {
            // swaybg is itself the long-running renderer (Section 3.6) -
            // replace any prior instance, then launch detached.
            runner_.terminate_by_program_name("swaybg");
            bool ok = true;
            for (const auto& cmd : commands) ok = runner_.run_detached(cmd) && ok;
            return ok ? platform_common::SetWallpaperResult::Success
                       : platform_common::SetWallpaperResult::Failed;
        }

        // Every other backend (gsettings/xfconf-query/plasma-apply-
        // wallpaperimage/feh) is a short-lived "set and exit" command.
        bool all_ok = true;
        for (const auto& cmd : commands) {
            auto result = runner_.run(cmd);
            if (result.spawn_failed || result.exit_code != 0) all_ok = false;
        }
        return all_ok ? platform_common::SetWallpaperResult::Success
                       : platform_common::SetWallpaperResult::Failed;
    }

    std::vector<platform_common::MonitorInfo> enumerate_monitors() const override {
        return enumerate_via_xrandr(runner_);
    }

    bool supports_per_monitor_wallpaper() const override {
        return supports_per_monitor(detected_de_);
    }

    std::string backend_name() const override {
        return std::string("linux-") + to_string(detected_de_);
    }

private:
    DesktopEnvironment detected_de_;
    mutable RealProcessRunner runner_;
};

} // namespace
} // namespace weatherpaper::platform_linux

namespace weatherpaper::platform_common {
std::unique_ptr<IPlatformWallpaper> create_platform_wallpaper() {
    return std::make_unique<platform_linux::LinuxPlatformWallpaper>();
}
} // namespace weatherpaper::platform_common
