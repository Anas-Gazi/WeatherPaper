// weatherpaper/platform_linux/backend.hpp
//
// Module: platform_linux (Section 3.6) - CORE LOGIC HEADER
//
// This is the highest-risk module in the whole codebase per the spec: "no
// single wallpaper API ... detect the running desktop environment ...
// dispatch to the correct backend". Everything in this header is PURE LOGIC
// (string/env-var in, WallpaperCommand struct out) so it is fully unit
// testable without a display server, a real DE, or spawning any process -
// exactly the property that makes it safe for community contributors to
// add a new DE backend and trust it via `ctest` alone before ever touching
// real hardware (see CONTRIBUTING.md "Adding a new Linux DE backend").
//
// SECURITY (Section 5): WallpaperCommand is an argv vector, NEVER a shell
// string. There is no string concatenation into a shell command line
// anywhere in this module - every backend function below returns
// {program, [arg0, arg1, ...]} and the caller (process_runner.cpp) executes
// it via posix_spawn/execvp with that exact argv, so a maliciously-named
// file (e.g. containing `; rm -rf ~` or `$(...)`) can never be interpreted
// by a shell - it is passed as a single opaque argv element.
#pragma once

#include <optional>
#include <string>
#include <vector>

#include "weatherpaper/scaling_and_fit/scaling_and_fit.hpp"
#include "weatherpaper/tag_system/tag_system.hpp"

namespace weatherpaper::platform_linux {

// --- Desktop environment detection ----------------------------------------

enum class DesktopEnvironment {
    Gnome,
    Cinnamon,
    Mate,
    Deepin,       // routed through the same gsettings-family backend as GNOME
    KdePlasma,
    Xfce,
    WaylandHyprland,
    WaylandSway,
    GenericX11,   // no recognized DE, X11 available -> feh fallback
    Unknown       // could not determine anything usable
};

[[nodiscard]] const char* to_string(DesktopEnvironment de) noexcept;

// Injectable environment-variable reader so detection is unit-testable
// without mutating the real process environment (tests build a
// FakeEnvReader; production uses RealEnvReader, which wraps std::getenv).
class IEnvReader {
public:
    virtual ~IEnvReader() = default;
    [[nodiscard]] virtual std::optional<std::string> get(const std::string& name) const = 0;
};

class RealEnvReader : public IEnvReader {
public:
    [[nodiscard]] std::optional<std::string> get(const std::string& name) const override;
};

// Detection order (Section 3.6): $XDG_CURRENT_DESKTOP first, then
// $DESKTOP_SESSION as fallback, then $XDG_SESSION_TYPE to distinguish a
// bare Wayland compositor from a bare X11 window manager when neither of
// the above identified a known DE.
[[nodiscard]] DesktopEnvironment detect_desktop_environment(const IEnvReader& env);

// --- Command building (Section 3.6 backend dispatch table) ---------------

struct WallpaperCommand {
    std::string program;             // e.g. "gsettings" - resolved via PATH by execvp,
                                      // never a shell-interpreted string
    std::vector<std::string> args;   // argv[1..], each element passed verbatim
    bool is_supported = true;        // false = this DE/asset-type combination
                                      // has no known backend (e.g. video on
                                      // a plain gsettings-only DE - caller
                                      // should fall back to a static frame)
};

struct FitMappingNote {
    // Human-readable note about the OS-native fit-mode equivalent used,
    // included in log output so a "why does GNOME show letterboxing when I
    // picked Fill" bug report is easy to diagnose (Section 3.6 diagnostics
    // intent extended to fit-mode mapping, which has the same "many
    // slightly different backends" problem as wallpaper-setting itself).
    std::string detail;
};

// Builds the concrete command sequence for setting a STATIC IMAGE
// wallpaper on the given desktop environment. Most backends need exactly
// one command; GNOME-family and XFCE need two (one to set the image, one
// to set the fit/style property) - hence a vector rather than a single
// WallpaperCommand. Commands must be executed in order. Returns a single
// element with is_supported=false if `de` has no image backend at all.
[[nodiscard]] std::vector<WallpaperCommand> build_static_image_command(
    DesktopEnvironment de,
    const std::string& absolute_image_path,
    scaling_and_fit::FitMode fit_mode);

// Builds the command sequence for a VIDEO/animated wallpaper where the
// desktop environment has a native looping-background tool (currently only
// the Wayland-compositor branches, via mpvpaper - Section 3.6). Returns a
// single element with is_supported=false for every DE-based backend
// (GNOME/KDE/XFCE/etc, which have no native video wallpaper concept) -
// render_engine must fall back to its own manual compositing window in
// that case (see render_engine.hpp).
[[nodiscard]] std::vector<WallpaperCommand> build_video_command(
    DesktopEnvironment de,
    const std::string& absolute_video_path,
    scaling_and_fit::FitMode fit_mode);

// True if `de`'s backend can meaningfully target one monitor independently
// (Section 3.4 per-monitor support - Linux support varies by DE).
[[nodiscard]] bool supports_per_monitor(DesktopEnvironment de) noexcept;

} // namespace weatherpaper::platform_linux
