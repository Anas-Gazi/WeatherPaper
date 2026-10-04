#include "weatherpaper/platform_linux/backend.hpp"

#include <cctype>
#include <cstdlib>

namespace weatherpaper::platform_linux {

using scaling_and_fit::FitMode;

const char* to_string(DesktopEnvironment de) noexcept {
    switch (de) {
        case DesktopEnvironment::Gnome:           return "gnome";
        case DesktopEnvironment::Cinnamon:        return "cinnamon";
        case DesktopEnvironment::Mate:            return "mate";
        case DesktopEnvironment::Deepin:          return "deepin";
        case DesktopEnvironment::KdePlasma:       return "kde-plasma";
        case DesktopEnvironment::Xfce:            return "xfce";
        case DesktopEnvironment::WaylandHyprland:  return "wayland-hyprland";
        case DesktopEnvironment::WaylandSway:      return "wayland-sway";
        case DesktopEnvironment::GenericX11:      return "generic-x11";
        case DesktopEnvironment::Unknown:         return "unknown";
    }
    return "unknown";
}

std::optional<std::string> RealEnvReader::get(const std::string& name) const {
    const char* v = std::getenv(name.c_str());
    if (v == nullptr || *v == '\0') return std::nullopt;
    return std::string(v);
}

namespace {
// Case-insensitive substring check - $XDG_CURRENT_DESKTOP can be a
// colon-separated list like "ubuntu:GNOME" or "X-Cinnamon", so we search
// rather than exact-match.
bool icontains(const std::string& haystack, const char* needle) {
    std::string h = haystack;
    for (auto& c : h) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    std::string n = needle;
    for (auto& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return h.find(n) != std::string::npos;
}
} // namespace

DesktopEnvironment detect_desktop_environment(const IEnvReader& env) {
    // 1. $XDG_CURRENT_DESKTOP - the primary, most reliable signal (Section 3.6).
    if (auto xcd = env.get("XDG_CURRENT_DESKTOP")) {
        if (icontains(*xcd, "kde"))       return DesktopEnvironment::KdePlasma;
        if (icontains(*xcd, "cinnamon"))  return DesktopEnvironment::Cinnamon;
        if (icontains(*xcd, "mate"))      return DesktopEnvironment::Mate;
        if (icontains(*xcd, "xfce"))      return DesktopEnvironment::Xfce;
        if (icontains(*xcd, "deepin"))    return DesktopEnvironment::Deepin;
        if (icontains(*xcd, "gnome"))     return DesktopEnvironment::Gnome;
        if (icontains(*xcd, "hyprland"))  return DesktopEnvironment::WaylandHyprland;
        if (icontains(*xcd, "sway"))      return DesktopEnvironment::WaylandSway;
    }

    // 2. $DESKTOP_SESSION - fallback signal (Section 3.6), some distros only set this.
    if (auto ds = env.get("DESKTOP_SESSION")) {
        if (icontains(*ds, "kde") || icontains(*ds, "plasma")) return DesktopEnvironment::KdePlasma;
        if (icontains(*ds, "cinnamon")) return DesktopEnvironment::Cinnamon;
        if (icontains(*ds, "mate"))     return DesktopEnvironment::Mate;
        if (icontains(*ds, "xfce"))     return DesktopEnvironment::Xfce;
        if (icontains(*ds, "deepin"))   return DesktopEnvironment::Deepin;
        if (icontains(*ds, "gnome"))    return DesktopEnvironment::Gnome;
    }

    // 3. Compositor-specific env vars, conventionally set by Hyprland/Sway
    // themselves regardless of XDG_CURRENT_DESKTOP - useful when a bare
    // compositor (no DE shell) doesn't bother setting either var above.
    if (env.get("HYPRLAND_INSTANCE_SIGNATURE")) return DesktopEnvironment::WaylandHyprland;
    if (env.get("SWAYSOCK"))                     return DesktopEnvironment::WaylandSway;

    // 4. Generic session-type fallback.
    if (auto st = env.get("XDG_SESSION_TYPE")) {
        if (*st == "wayland") {
            // Bare Wayland compositor we don't specifically recognize.
            // ASSUMPTION: default to the Hyprland bucket purely as a label -
            // build_static_image_command()'s Wayland branches use identical
            // tooling (swaybg/swww/mpvpaper) for both enum values, so this
            // only affects the diagnostic log line, never behavior.
            return DesktopEnvironment::WaylandHyprland;
        }
        if (*st == "x11" && env.get("DISPLAY")) {
            return DesktopEnvironment::GenericX11;
        }
    }
    if (env.get("DISPLAY")) return DesktopEnvironment::GenericX11; // last-resort X11 guess

    return DesktopEnvironment::Unknown;
}

namespace {

std::string file_uri(const std::string& absolute_path) {
    return "file://" + absolute_path; // ASSUMPTION: paths are validated
                                       // absolute + sanitized by the caller
                                       // (asset_manager) before reaching
                                       // here - see Section 5 path
                                       // validation requirement.
}

// --- GNOME-family (GNOME, Cinnamon, MATE, Deepin best-effort) -----------

const char* gnome_family_picture_options(FitMode mode) {
    switch (mode) {
        case FitMode::Fill:    return "zoom";
        case FitMode::Fit:     return "scaled";
        case FitMode::Stretch: return "stretched";
        case FitMode::Center:  return "centered";
        case FitMode::Tile:    return "wallpaper";
    }
    return "zoom";
}

std::vector<WallpaperCommand> build_gnome(const std::string& path, FitMode mode) {
    return {
        {"gsettings", {"set", "org.gnome.desktop.background", "picture-uri", file_uri(path)}},
        // Also set the dark-mode key so GNOME 42+ (which reads
        // picture-uri-dark when a dark color-scheme is active) stays
        // consistent - harmless no-op on older GNOME.
        {"gsettings", {"set", "org.gnome.desktop.background", "picture-uri-dark", file_uri(path)}},
        {"gsettings", {"set", "org.gnome.desktop.background", "picture-options",
                        gnome_family_picture_options(mode)}},
    };
}

std::vector<WallpaperCommand> build_cinnamon(const std::string& path, FitMode mode) {
    return {
        {"gsettings", {"set", "org.cinnamon.desktop.background", "picture-uri", file_uri(path)}},
        {"gsettings", {"set", "org.cinnamon.desktop.background", "picture-options",
                        gnome_family_picture_options(mode)}},
    };
}

std::vector<WallpaperCommand> build_mate(const std::string& path, FitMode mode) {
    // MATE's schema takes a plain filesystem path (no file:// prefix) under
    // a differently-named key.
    const char* style = "zoom";
    switch (mode) {
        case FitMode::Fill:    style = "zoom"; break;
        case FitMode::Fit:     style = "scaled"; break;
        case FitMode::Stretch: style = "stretched"; break;
        case FitMode::Center:  style = "centered"; break;
        case FitMode::Tile:    style = "wallpaper"; break;
    }
    return {
        {"gsettings", {"set", "org.mate.background", "picture-filename", path}},
        {"gsettings", {"set", "org.mate.background", "picture-options", style}},
    };
}

std::vector<WallpaperCommand> build_deepin(const std::string& path, FitMode /*mode*/) {
    // ASSUMPTION / KNOWN LIMITATION: Deepin (DDE) does not reliably expose
    // the same gsettings schema across releases; some DDE versions ship a
    // GNOME-compatible org.gnome.desktop.background schema (common on
    // Deepin derivatives), which is what this best-effort branch targets.
    // Community contributors on real Deepin hardware are specifically
    // invited (see CONTRIBUTING.md) to replace this with the native
    // `dbus-send --dest=com.deepin.daemon.Appearance ...` call if it proves
    // unreliable - flagged rather than silently guessed-and-shipped as
    // "done".
    return {
        {"gsettings", {"set", "org.gnome.desktop.background", "picture-uri", file_uri(path)}},
    };
}

// --- KDE Plasma -----------------------------------------------------------

std::vector<WallpaperCommand> build_kde(const std::string& path, FitMode /*mode*/) {
    // Section 3.6: "KDE Plasma -> qdbus/plasma-apply-wallpaperimage".
    // plasma-apply-wallpaperimage (Plasma 5.24+) is the simplest, safest
    // (single argv, no embedded script) option and is what we use.
    // KNOWN LIMITATION: this CLI tool does not expose a fit-mode flag -
    // Plasma's "FillMode" is a property of the wallpaper QML plugin
    // configuration, settable only via the more complex
    // `qdbus org.kde.plasmashell /PlasmaShell evaluateScript "<js>"`
    // approach. That JS-eval path is intentionally NOT used here even
    // though it could pass fit_mode through, because the script argument
    // would need extremely careful escaping to stay injection-safe end to
    // end - a good candidate for a reviewed follow-up PR (see
    // CONTRIBUTING.md) rather than something to rush into v1.
    return {
        {"plasma-apply-wallpaperimage", {path}},
    };
}

// --- XFCE -------------------------------------------------------------

int xfce_image_style(FitMode mode) {
    // xfconf-query image-style integer values (xfdesktop):
    // 0=None 1=Centered 2=Tiled 3=Stretched 4=Scaled 5=Zoomed 6=Zoomed Fill
    switch (mode) {
        case FitMode::Fill:    return 6;
        case FitMode::Fit:     return 4; // "Scaled" preserves aspect, closest to Fit
        case FitMode::Stretch: return 3;
        case FitMode::Center:  return 1;
        case FitMode::Tile:    return 2;
    }
    return 6;
}

std::vector<WallpaperCommand> build_xfce(const std::string& path, FitMode mode) {
    // KNOWN LIMITATION: XFCE's xfconf property path encodes the monitor
    // and workspace name (e.g. "monitorVGA-1", "workspace0"), which is not
    // knowable without querying xfconf first. This targets the common
    // single-monitor default property some xfdesktop versions still
    // honor as a fallback; asset_manager/render_engine should treat a
    // non-zero exit code from this command as "per-monitor property names
    // differ on this system" and is a documented, tracked limitation (see
    // CONTRIBUTING.md "Adding a new Linux DE backend" for how a
    // contributor can make this properly enumerate monitors via
    // `xfconf-query -c xfce4-desktop -l`).
    const std::string base = "/backdrop/screen0/monitor0/workspace0";
    return {
        {"xfconf-query", {"-c", "xfce4-desktop", "-p", base + "/last-image", "-s", path}},
        {"xfconf-query", {"-c", "xfce4-desktop", "-p", base + "/image-style", "-s",
                           std::to_string(xfce_image_style(mode))}},
    };
}

// --- Wayland compositors without a DE shell (Hyprland, Sway) -----------

const char* swaybg_mode(FitMode mode) {
    // swaybg's -m flag accepts exactly: stretch, fit, fill, center, tile -
    // a rare case where the upstream tool's vocabulary maps 1:1 onto ours.
    switch (mode) {
        case FitMode::Fill:    return "fill";
        case FitMode::Fit:     return "fit";
        case FitMode::Stretch: return "stretch";
        case FitMode::Center:  return "center";
        case FitMode::Tile:    return "tile";
    }
    return "fill";
}

std::vector<WallpaperCommand> build_wayland_static(const std::string& path, FitMode mode) {
    // swaybg replacing any previously-running instance is NOT automatic -
    // the caller (wallpaper_linux.cpp) is responsible for terminating a
    // prior swaybg process before launching a new one, since swaybg has no
    // "update image" IPC of its own (unlike swww). Documented at the call
    // site.
    return {
        {"swaybg", {"-i", path, "-m", swaybg_mode(mode)}},
    };
}

std::vector<WallpaperCommand> build_wayland_video(const std::string& path, FitMode /*mode*/) {
    // mpvpaper Section 3.6: "mpvpaper for video". "*" targets every output;
    // per-monitor targeting would pass a specific output name instead
    // (Section 3.4 - platform_linux::supports_per_monitor covers this).
    return {
        {"mpvpaper", {"-o", "no-audio loop", "*", path}},
    };
}

// --- Generic X11 (no recognized DE) -------------------------------------

const char* feh_flag(FitMode mode) {
    switch (mode) {
        case FitMode::Fill:    return "--bg-fill";
        case FitMode::Fit:     return "--bg-max";    // closest: shows whole image, may letterbox
        case FitMode::Stretch: return "--bg-scale";
        case FitMode::Center:  return "--bg-center";
        case FitMode::Tile:    return "--bg-tile";
    }
    return "--bg-fill";
}

std::vector<WallpaperCommand> build_feh(const std::string& path, FitMode mode) {
    return {
        {"feh", {feh_flag(mode), path}},
    };
}

} // namespace

std::vector<WallpaperCommand> build_static_image_command(
    DesktopEnvironment de, const std::string& absolute_image_path, FitMode fit_mode) {
    switch (de) {
        case DesktopEnvironment::Gnome:            return build_gnome(absolute_image_path, fit_mode);
        case DesktopEnvironment::Cinnamon:         return build_cinnamon(absolute_image_path, fit_mode);
        case DesktopEnvironment::Mate:             return build_mate(absolute_image_path, fit_mode);
        case DesktopEnvironment::Deepin:           return build_deepin(absolute_image_path, fit_mode);
        case DesktopEnvironment::KdePlasma:        return build_kde(absolute_image_path, fit_mode);
        case DesktopEnvironment::Xfce:             return build_xfce(absolute_image_path, fit_mode);
        case DesktopEnvironment::WaylandHyprland:  return build_wayland_static(absolute_image_path, fit_mode);
        case DesktopEnvironment::WaylandSway:      return build_wayland_static(absolute_image_path, fit_mode);
        case DesktopEnvironment::GenericX11:       return build_feh(absolute_image_path, fit_mode);
        case DesktopEnvironment::Unknown:          break;
    }
    return { WallpaperCommand{"", {}, /*is_supported=*/false} };
}

std::vector<WallpaperCommand> build_video_command(
    DesktopEnvironment de, const std::string& absolute_video_path, FitMode fit_mode) {
    switch (de) {
        case DesktopEnvironment::WaylandHyprland:
        case DesktopEnvironment::WaylandSway:
            return build_wayland_video(absolute_video_path, fit_mode);
        default:
            // No DE-native video wallpaper support - render_engine falls
            // back to manual frame compositing (Section 3.4).
            return { WallpaperCommand{"", {}, /*is_supported=*/false} };
    }
}

bool supports_per_monitor(DesktopEnvironment de) noexcept {
    // Section 3.4: "Linux support varies by DE - degrade gracefully".
    // mpvpaper/swaybg CAN target a specific output name, so Wayland
    // compositors are marked supported; the gsettings/xfconf/KDE backends
    // above apply one wallpaper for the whole desktop with no
    // per-monitor addressing in the simple commands built here.
    return de == DesktopEnvironment::WaylandHyprland || de == DesktopEnvironment::WaylandSway;
}

} // namespace weatherpaper::platform_linux
