// Concrete Linux implementation of platform_common::IPlatformHooks.
//
// DESIGN NOTE: unlike wallpaper_linux.cpp (which must run arbitrary
// user/theme-pack-derived file paths through argv-only exec - Section 5),
// this file only ever runs a small number of FIXED, LITERAL, hardcoded
// command strings with zero user-controlled interpolation (e.g. always
// exactly "loginctl show-session self -p LockedHint --value", never built
// from a variable). That is why popen() is acceptable here specifically -
// there is no injection surface because there is no dynamic input - while
// wallpaper_linux.cpp and process_runner.cpp never use popen()/system() at
// all, precisely because they DO handle user/theme-supplied paths.
//
// KNOWN LIMITATION (see CONTRIBUTING.md "good first issue" list): this is a
// polling implementation (driven by pump_events(), called periodically by
// the app's own OS timer per Section 2.2 - no busy loop of its own). A
// fully event-driven version would subscribe to
// org.freedesktop.ScreenSaver / org.freedesktop.login1 D-Bus signals via
// libsystemd or sd-bus, avoiding the ~1s worst-case latency a poll implies;
// that is intentionally left as future work rather than adding a heavy
// D-Bus client dependency to hit the <25MB installer budget in v1.
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "weatherpaper/platform_common/platform_common.hpp"

namespace weatherpaper::platform_linux {
namespace {

namespace fs = std::filesystem;

// Runs a FIXED, LITERAL command (never built from user input - see file
// header) and returns its trimmed stdout, or empty string on any failure.
std::string capture_fixed_command(const char* literal_command) {
    std::array<char, 256> buffer{};
    std::string result;
    FILE* pipe = popen(literal_command, "r"); // NOLINT - see file header
    if (pipe == nullptr) return result;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r' || result.back() == ' ')) {
        result.pop_back();
    }
    return result;
}

platform_common::PowerState read_power_state_from_sysfs() {
    bool any_ac_online = false;
    bool any_power_supply_found = false;
    int lowest_capacity = 100;

    std::error_code ec;
    if (fs::exists("/sys/class/power_supply", ec) && !ec) {
        for (const auto& entry : fs::directory_iterator("/sys/class/power_supply", ec)) {
            any_power_supply_found = true;
            const auto type_path = entry.path() / "type";
            std::ifstream type_file(type_path);
            std::string type;
            if (type_file.is_open()) std::getline(type_file, type);

            if (type == "Mains" || type == "USB") {
                std::ifstream online_file(entry.path() / "online");
                int online = 0;
                if (online_file.is_open()) online_file >> online;
                if (online == 1) any_ac_online = true;
            } else if (type == "Battery") {
                std::ifstream cap_file(entry.path() / "capacity");
                int capacity = 100;
                if (cap_file.is_open()) cap_file >> capacity;
                lowest_capacity = std::min(lowest_capacity, capacity);
            }
        }
    }

    if (!any_power_supply_found) {
        // Desktop with no battery at all - always effectively "on AC".
        return platform_common::PowerState::OnACPower;
    }
    if (any_ac_online) return platform_common::PowerState::OnACPower;

    // ASSUMPTION: without a D-Bus dependency on power-profiles-daemon
    // (see file header), we approximate "battery saver" as "on battery
    // AND capacity below 20%" - a reasonable proxy since most desktop
    // environments auto-enable their own battery saver around then, even
    // if the user hasn't explicitly toggled a power-saver mode.
    return lowest_capacity <= 20 ? platform_common::PowerState::OnBatterySaver
                                   : platform_common::PowerState::OnBatteryNormal;
}

bool read_session_locked_state() {
    // Fixed literal command - see file header for why popen is safe here.
    std::string out = capture_fixed_command(
        "loginctl show-session \"$(loginctl show-user \"$(id -un)\" -p Display --value)\" "
        "-p LockedHint --value 2>/dev/null");
    return out == "yes";
}

bool read_any_network_interface_up() {
    std::error_code ec;
    if (!fs::exists("/sys/class/net", ec) || ec) return true; // fail open: assume "up"
    for (const auto& entry : fs::directory_iterator("/sys/class/net", ec)) {
        if (entry.path().filename() == "lo") continue; // skip loopback
        std::ifstream state_file(entry.path() / "operstate");
        std::string state;
        if (state_file.is_open()) std::getline(state_file, state);
        if (state == "up") return true;
    }
    return false;
}

class LinuxPlatformHooks : public platform_common::IPlatformHooks {
public:
    LinuxPlatformHooks()
        : last_power_state_(read_power_state_from_sysfs()),
          last_locked_(read_session_locked_state()),
          last_network_up_(read_any_network_interface_up()) {}

    void on_session_locked(VoidCallback cb) override { locked_cbs_.push_back(std::move(cb)); }
    void on_session_unlocked(VoidCallback cb) override { unlocked_cbs_.push_back(std::move(cb)); }
    void on_power_state_changed(PowerStateCallback cb) override { power_cbs_.push_back(std::move(cb)); }
    void on_network_reconnected(VoidCallback cb) override { network_cbs_.push_back(std::move(cb)); }

    platform_common::PowerState current_power_state() const override { return last_power_state_; }

    bool is_fullscreen_app_active() const override {
        // KNOWN LIMITATION: no universal Wayland API for this (each
        // compositor would need its own query - Hyprland has
        // `hyprctl activewindow`, Sway has `swaymsg -t get_tree`). This
        // X11/XWayland-only check via wmctrl is a documented best-effort;
        // failing to detect fullscreen just means animated wallpapers
        // keep rendering during a fullscreen game on an unsupported
        // compositor (a performance annoyance, not a correctness bug) -
        // tracked as a CONTRIBUTING.md "good first issue".
        std::string out = capture_fixed_command(
            "xprop -root _NET_ACTIVE_WINDOW 2>/dev/null | grep -o '0x[0-9a-f]*' | head -1");
        if (out.empty()) return false;
        std::string cmd = "xprop -id " + out + " _NET_WM_STATE 2>/dev/null";
        // NOTE: `out` here is a window ID captured from our OWN fixed
        // xprop query above (a hex number produced by X11 itself), never
        // user- or file-supplied data, so concatenating it into this
        // second fixed-shape command does not reintroduce the injection
        // risk this module otherwise avoids (Section 5) - it cannot
        // contain shell metacharacters because xprop only ever emits a
        // "0x" + hex-digits token here.
        std::string state = capture_fixed_command(cmd.c_str());
        return state.find("_NET_WM_STATE_FULLSCREEN") != std::string::npos;
    }

    void pump_events() override {
        auto power = read_power_state_from_sysfs();
        if (power != last_power_state_) {
            last_power_state_ = power;
            for (auto& cb : power_cbs_) cb(power);
        }

        bool locked = read_session_locked_state();
        if (locked != last_locked_) {
            last_locked_ = locked;
            for (auto& cb : (locked ? locked_cbs_ : unlocked_cbs_)) cb();
        }

        bool net_up = read_any_network_interface_up();
        if (net_up && !last_network_up_) {
            for (auto& cb : network_cbs_) cb();
        }
        last_network_up_ = net_up;
    }

private:
    platform_common::PowerState last_power_state_;
    bool last_locked_;
    bool last_network_up_;
    std::vector<VoidCallback> locked_cbs_;
    std::vector<VoidCallback> unlocked_cbs_;
    std::vector<PowerStateCallback> power_cbs_;
    std::vector<VoidCallback> network_cbs_;
};

} // namespace
} // namespace weatherpaper::platform_linux

namespace weatherpaper::platform_common {
std::unique_ptr<IPlatformHooks> create_platform_hooks() {
    return std::make_unique<platform_linux::LinuxPlatformHooks>();
}
} // namespace weatherpaper::platform_common
