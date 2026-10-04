#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include <unordered_map>

#include "weatherpaper/platform_linux/backend.hpp"

using namespace weatherpaper::platform_linux;
using weatherpaper::scaling_and_fit::FitMode;

namespace {
// Injectable fake environment - see backend.hpp's IEnvReader doc comment.
class FakeEnvReader : public IEnvReader {
public:
    std::unordered_map<std::string, std::string> values;
    std::optional<std::string> get(const std::string& name) const override {
        auto it = values.find(name);
        if (it == values.end()) return std::nullopt;
        return it->second;
    }
};

// Helper: find an arg in a command's argv, used to assert "the file path
// appears as a single opaque argv element", the core anti-injection claim.
bool contains_arg(const WallpaperCommand& cmd, const std::string& needle) {
    for (const auto& a : cmd.args) if (a == needle) return true;
    return false;
}
} // namespace

// --- Desktop environment detection ----------------------------------------

TEST_CASE("detect_desktop_environment: GNOME via XDG_CURRENT_DESKTOP") {
    FakeEnvReader env;
    env.values["XDG_CURRENT_DESKTOP"] = "ubuntu:GNOME";
    CHECK(detect_desktop_environment(env) == DesktopEnvironment::Gnome);
}

TEST_CASE("detect_desktop_environment: KDE Plasma variants") {
    FakeEnvReader env;
    env.values["XDG_CURRENT_DESKTOP"] = "KDE";
    CHECK(detect_desktop_environment(env) == DesktopEnvironment::KdePlasma);
}

TEST_CASE("detect_desktop_environment: XFCE, Cinnamon, MATE all recognized") {
    FakeEnvReader env;
    env.values["XDG_CURRENT_DESKTOP"] = "XFCE";
    CHECK(detect_desktop_environment(env) == DesktopEnvironment::Xfce);

    env.values["XDG_CURRENT_DESKTOP"] = "X-Cinnamon";
    CHECK(detect_desktop_environment(env) == DesktopEnvironment::Cinnamon);

    env.values["XDG_CURRENT_DESKTOP"] = "MATE";
    CHECK(detect_desktop_environment(env) == DesktopEnvironment::Mate);
}

TEST_CASE("detect_desktop_environment: falls back to DESKTOP_SESSION when XDG_CURRENT_DESKTOP is unset") {
    FakeEnvReader env;
    env.values["DESKTOP_SESSION"] = "plasma";
    CHECK(detect_desktop_environment(env) == DesktopEnvironment::KdePlasma);
}

TEST_CASE("detect_desktop_environment: Hyprland via compositor-specific env var, no XDG hints at all") {
    FakeEnvReader env;
    env.values["HYPRLAND_INSTANCE_SIGNATURE"] = "abc123";
    CHECK(detect_desktop_environment(env) == DesktopEnvironment::WaylandHyprland);
}

TEST_CASE("detect_desktop_environment: Sway via SWAYSOCK") {
    FakeEnvReader env;
    env.values["SWAYSOCK"] = "/run/user/1000/sway-ipc.sock";
    CHECK(detect_desktop_environment(env) == DesktopEnvironment::WaylandSway);
}

TEST_CASE("detect_desktop_environment: bare X11 window manager falls back to GenericX11") {
    FakeEnvReader env;
    env.values["XDG_SESSION_TYPE"] = "x11";
    env.values["DISPLAY"] = ":0";
    CHECK(detect_desktop_environment(env) == DesktopEnvironment::GenericX11);
}

TEST_CASE("detect_desktop_environment: nothing set at all -> Unknown") {
    FakeEnvReader env;
    CHECK(detect_desktop_environment(env) == DesktopEnvironment::Unknown);
}

// --- Command building: correctness AND anti-injection (Section 5) --------

TEST_CASE("GNOME: builds gsettings picture-uri + picture-options, path as single argv element") {
    auto cmds = build_static_image_command(DesktopEnvironment::Gnome,
                                             "/home/user/wallpapers/rain.jpg", FitMode::Fill);
    REQUIRE(cmds.size() >= 2);
    for (auto& c : cmds) CHECK(c.program == "gsettings");
    CHECK(contains_arg(cmds[0], "file:///home/user/wallpapers/rain.jpg"));
    CHECK(contains_arg(cmds.back(), "zoom")); // Fill -> "zoom"
}

TEST_CASE("GNOME fit-mode mapping covers all 5 FitModes distinctly") {
    auto fill = build_static_image_command(DesktopEnvironment::Gnome, "/x.jpg", FitMode::Fill);
    auto fit = build_static_image_command(DesktopEnvironment::Gnome, "/x.jpg", FitMode::Fit);
    auto stretch = build_static_image_command(DesktopEnvironment::Gnome, "/x.jpg", FitMode::Stretch);
    auto center = build_static_image_command(DesktopEnvironment::Gnome, "/x.jpg", FitMode::Center);
    auto tile = build_static_image_command(DesktopEnvironment::Gnome, "/x.jpg", FitMode::Tile);
    CHECK(contains_arg(fill.back(), "zoom"));
    CHECK(contains_arg(fit.back(), "scaled"));
    CHECK(contains_arg(stretch.back(), "stretched"));
    CHECK(contains_arg(center.back(), "centered"));
    CHECK(contains_arg(tile.back(), "wallpaper"));
}

TEST_CASE("KDE: uses plasma-apply-wallpaperimage with the path as its sole argument") {
    auto cmds = build_static_image_command(DesktopEnvironment::KdePlasma,
                                             "/home/user/storm.png", FitMode::Fill);
    REQUIRE(cmds.size() == 1);
    CHECK(cmds[0].program == "plasma-apply-wallpaperimage");
    CHECK(cmds[0].args.size() == 1);
    CHECK(cmds[0].args[0] == "/home/user/storm.png");
}

TEST_CASE("XFCE: sets last-image and image-style via xfconf-query, no shell string built") {
    auto cmds = build_static_image_command(DesktopEnvironment::Xfce, "/x/snow.jpg", FitMode::Center);
    REQUIRE(cmds.size() == 2);
    for (auto& c : cmds) CHECK(c.program == "xfconf-query");
    CHECK(contains_arg(cmds[0], "/x/snow.jpg"));
    CHECK(contains_arg(cmds[1], "1")); // Center -> image-style 1
}

TEST_CASE("Wayland (Hyprland/Sway): static uses swaybg with a direct 1:1 fit-mode flag") {
    for (auto de : {DesktopEnvironment::WaylandHyprland, DesktopEnvironment::WaylandSway}) {
        auto cmds = build_static_image_command(de, "/x/fog.jpg", FitMode::Tile);
        REQUIRE(cmds.size() == 1);
        CHECK(cmds[0].program == "swaybg");
        CHECK(contains_arg(cmds[0], "/x/fog.jpg"));
        CHECK(contains_arg(cmds[0], "tile"));
    }
}

TEST_CASE("Wayland: video uses mpvpaper targeting all outputs by default") {
    auto cmds = build_video_command(DesktopEnvironment::WaylandSway, "/x/storm.mp4", FitMode::Fill);
    REQUIRE(cmds.size() == 1);
    CHECK(cmds[0].program == "mpvpaper");
    CHECK(contains_arg(cmds[0], "*"));
    CHECK(contains_arg(cmds[0], "/x/storm.mp4"));
}

TEST_CASE("Non-Wayland DEs report video as unsupported (render_engine must fall back)") {
    for (auto de : {DesktopEnvironment::Gnome, DesktopEnvironment::KdePlasma,
                     DesktopEnvironment::Xfce, DesktopEnvironment::Mate,
                     DesktopEnvironment::GenericX11}) {
        auto cmds = build_video_command(de, "/x/rain.mp4", FitMode::Fill);
        REQUIRE(cmds.size() == 1);
        CHECK_FALSE(cmds[0].is_supported);
    }
}

TEST_CASE("GenericX11: uses feh with the correct --bg-* flag per fit mode") {
    auto fill = build_static_image_command(DesktopEnvironment::GenericX11, "/x.jpg", FitMode::Fill);
    CHECK(fill[0].program == "feh");
    CHECK(contains_arg(fill[0], "--bg-fill"));

    auto tile = build_static_image_command(DesktopEnvironment::GenericX11, "/x.jpg", FitMode::Tile);
    CHECK(contains_arg(tile[0], "--bg-tile"));
}

TEST_CASE("Unknown desktop environment reports unsupported rather than guessing") {
    auto cmds = build_static_image_command(DesktopEnvironment::Unknown, "/x.jpg", FitMode::Fill);
    REQUIRE(cmds.size() == 1);
    CHECK_FALSE(cmds[0].is_supported);
}

TEST_CASE("SECURITY: a malicious-looking filename never produces extra argv elements or gets split") {
    // The core anti-injection property (Section 5): a path containing shell
    // metacharacters must survive as ONE argv string, not be split/
    // interpreted. We can't spawn a shell here to prove non-execution (no
    // shell is ever invoked - that's the whole design), but we CAN prove
    // the dangerous string is never fragmented across multiple argv
    // elements, which is what a string-concatenation bug would produce.
    const std::string evil = "/tmp/x; rm -rf ~ #.jpg";
    auto cmds = build_static_image_command(DesktopEnvironment::GenericX11, evil, FitMode::Fill);
    REQUIRE(cmds.size() == 1);
    REQUIRE(cmds[0].args.size() == 2); // ["--bg-fill", evil] - exactly 2 elements
    CHECK(cmds[0].args[1] == evil);    // the whole malicious string is ONE argv element
}

TEST_CASE("supports_per_monitor: true only for Wayland compositor backends in this implementation") {
    CHECK(supports_per_monitor(DesktopEnvironment::WaylandHyprland));
    CHECK(supports_per_monitor(DesktopEnvironment::WaylandSway));
    CHECK_FALSE(supports_per_monitor(DesktopEnvironment::Gnome));
    CHECK_FALSE(supports_per_monitor(DesktopEnvironment::KdePlasma));
    CHECK_FALSE(supports_per_monitor(DesktopEnvironment::Xfce));
}

TEST_CASE("to_string produces a distinct, log-friendly label for every DE") {
    CHECK(std::string(to_string(DesktopEnvironment::Gnome)) == "gnome");
    CHECK(std::string(to_string(DesktopEnvironment::WaylandHyprland)) == "wayland-hyprland");
    CHECK(std::string(to_string(DesktopEnvironment::Unknown)) == "unknown");
}
