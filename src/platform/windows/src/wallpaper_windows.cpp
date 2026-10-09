// Concrete Windows implementation of platform_common::IPlatformWallpaper,
// using the native IDesktopWallpaper COM interface (available identically
// on Windows 10 and Windows 11, Home and Pro - Section 3.5: "no edition-
// specific branching needed"). This interface natively supports:
//   - per-monitor wallpaper (SetWallpaper takes a monitor device path)
//   - native scaling/fit modes (SetPosition), so per Section 3.4a we pass
//     the user's chosen FitMode straight through rather than pre-scaling
//     the image ourselves.
//
// BUILD NOTE FOR CONTRIBUTORS: this file only compiles under MSVC or
// MinGW-w64 with the Windows SDK (requires <shobjidl.h> for
// IDesktopWallpaper, <wtsapi32.h>, <netlistmgr.h>). It was written and
// reviewed against the documented Win32/COM API surface but has NOT been
// compiled or run on real Windows hardware as part of producing this
// repository (no Windows toolchain was available in that environment) -
// see docs/ARCHITECTURE.md "Verification status" for exactly what has and
// hasn't been build/run-verified. Please treat this file as a careful
// first draft that needs a real Windows CI run (see .github/workflows) and
// hands-on testing on Windows 10 and 11 before it's trusted in a release.
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h> // Microsoft::WRL::ComPtr

#include <sstream>
#include <iostream>

#include "weatherpaper/platform_common/platform_common.hpp"

namespace weatherpaper::platform_windows {
namespace {

using Microsoft::WRL::ComPtr;
using scaling_and_fit::FitMode;

DESKTOP_WALLPAPER_POSITION to_native_position(FitMode mode) {
    // Section 3.4a: pass the user's fit-mode straight through to the OS
    // API rather than pre-scaling the file ourselves.
    switch (mode) {
        case FitMode::Fill:    return DWPOS_FILL;
        case FitMode::Fit:     return DWPOS_FIT;
        case FitMode::Stretch: return DWPOS_STRETCH;
        case FitMode::Center:  return DWPOS_CENTER;
        case FitMode::Tile:    return DWPOS_TILE;
    }
    return DWPOS_FILL;
}

std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return {};
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(len), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w.data(), len);
    if (!w.empty() && w.back() == L'\0') w.pop_back();
    return w;
}

std::string wide_to_utf8(const wchar_t* s) {
    if (s == nullptr) return {};
    int len = WideCharToMultiByte(CP_UTF8, 0, s, -1, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(len), '\0');
    WideCharToMultiByte(CP_UTF8, 0, s, -1, out.data(), len, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') out.pop_back();
    return out;
}

class WindowsPlatformWallpaper : public platform_common::IPlatformWallpaper {
public:
    WindowsPlatformWallpaper() {
        // COM apartment initialization is the app's responsibility (single
        // CoInitializeEx call at process startup, in src/app/main.cpp) -
        // this class assumes it has already happened, consistent with
        // "one COM apartment per process" best practice rather than each
        // module initializing/uninitializing COM independently.
        HRESULT hr = CoCreateInstance(CLSID_DesktopWallpaper, nullptr, CLSCTX_ALL,
                                        IID_PPV_ARGS(&wallpaper_));
        com_ok_ = SUCCEEDED(hr);
if (!com_ok_) {
    std::cerr << "[weatherpaper] CoCreateInstance failed: 0x"
              << std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
}
    }

    platform_common::SetWallpaperResult set_static_wallpaper(
        const platform_common::WallpaperRequest& request) override {
        if (!com_ok_ || wallpaper_ == nullptr) {
            return platform_common::SetWallpaperResult::Failed;
        }
        if (request.asset_type == tag_system::AssetType::Video) {
            // Section 3.4: video wallpapers are rendered by render_engine's
            // own compositing window (a WorkerW-technique overlay - see
            // render_engine.hpp), NOT via IDesktopWallpaper, which only
            // ever accepts a static image. render_engine must not call
            // this method for video assets.
            return platform_common::SetWallpaperResult::UnsupportedOnThisDesktop;
        }

        const std::wstring wpath = utf8_to_wide(request.file_path);
        wallpaper_->SetPosition(to_native_position(request.fit_mode));

        if (request.target_monitor_id.empty()) {
            // Apply to every monitor (Section 3.4 default / no per-monitor
            // override requested).
            UINT count = 0;
            wallpaper_->GetMonitorDevicePathCount(&count);
            bool any_ok = false;
            for (UINT i = 0; i < count; ++i) {
                LPWSTR device_path = nullptr;
                if (SUCCEEDED(wallpaper_->GetMonitorDevicePathAt(i, &device_path))) {
                    HRESULT hr = wallpaper_->SetWallpaper(device_path, wpath.c_str());
                    any_ok = any_ok || SUCCEEDED(hr);
                    CoTaskMemFree(device_path);
                }
            }
            // Fall back to the "apply to all monitors" nullptr-path call if
            // enumeration produced nothing (single-monitor systems
            // sometimes report zero paths for the primary display).
            if (count == 0) {
                HRESULT hr = wallpaper_->SetWallpaper(nullptr, wpath.c_str());
                any_ok = SUCCEEDED(hr);
            }
            return any_ok ? platform_common::SetWallpaperResult::Success
                            : platform_common::SetWallpaperResult::Failed;
        }

        // Per-monitor targeting (Section 3.4 - Windows natively supports this).
        std::wstring target = utf8_to_wide(request.target_monitor_id);
        HRESULT hr = wallpaper_->SetWallpaper(target.c_str(), wpath.c_str());
        return SUCCEEDED(hr) ? platform_common::SetWallpaperResult::Success
                               : platform_common::SetWallpaperResult::Failed;
    }

    std::vector<platform_common::MonitorInfo> enumerate_monitors() const override {
        std::vector<platform_common::MonitorInfo> monitors;
        if (!com_ok_ || wallpaper_ == nullptr) return monitors;

        UINT count = 0;
        wallpaper_->GetMonitorDevicePathCount(&count);
        for (UINT i = 0; i < count; ++i) {
            LPWSTR device_path = nullptr;
            if (FAILED(wallpaper_->GetMonitorDevicePathAt(i, &device_path))) continue;

            RECT rect{};
            wallpaper_->GetMonitorRECT(device_path, &rect);

            platform_common::MonitorInfo info;
            info.id = wide_to_utf8(device_path);
            info.friendly_name = info.id; // IDesktopWallpaper doesn't expose a
                                            // separate friendly name; the app
                                            // layer may cross-reference
                                            // EnumDisplayMonitors for a nicer
                                            // label in the settings UI.
            info.x = rect.left;
            info.y = rect.top;
            info.width = rect.right - rect.left;
            info.height = rect.bottom - rect.top;
            info.is_primary = (i == 0); // ASSUMPTION: index 0 is treated as
                                          // primary; IDesktopWallpaper does
                                          // not explicitly flag a primary
                                          // monitor, but in practice Windows
                                          // lists the primary display first.
            monitors.push_back(info);
            CoTaskMemFree(device_path);
        }
        return monitors;
    }

    bool supports_per_monitor_wallpaper() const override { return true; }

    std::string backend_name() const override { return "windows-desktopwallpaper-api"; }

private:
    ComPtr<IDesktopWallpaper> wallpaper_;
    bool com_ok_ = false;
};

} // namespace
} // namespace weatherpaper::platform_windows

namespace weatherpaper::platform_common {
std::unique_ptr<IPlatformWallpaper> create_platform_wallpaper() {
    return std::make_unique<platform_windows::WindowsPlatformWallpaper>();
}
} // namespace weatherpaper::platform_common

#endif // _WIN32
