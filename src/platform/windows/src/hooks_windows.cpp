// Concrete Windows implementation of platform_common::IPlatformHooks.
// See wallpaper_windows.cpp's file-header BUILD NOTE - the same
// verification-status caveat applies here: written against the documented
// Win32/COM API surface, not yet compiled/run on real Windows hardware.
#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wtsapi32.h>
#include <shellapi.h>
#include <netlistmgr.h>
#include <wrl/client.h>

#include "weatherpaper/platform_common/platform_common.hpp"

#pragma comment(lib, "Wtsapi32.lib")

namespace weatherpaper::platform_windows {
namespace {

using Microsoft::WRL::ComPtr;

platform_common::PowerState read_power_state() {
    SYSTEM_POWER_STATUS status{};
    if (!GetSystemPowerStatus(&status)) {
        return platform_common::PowerState::OnACPower; // fail open: assume mains power
    }
    if (status.ACLineStatus == 1) {
        return platform_common::PowerState::OnACPower;
    }
    // SYSTEM_POWER_STATUS has no direct "Battery Saver" flag pre-Win8.1;
    // Windows 10/11 expose it via SYSTEM_POWER_STATUS::SystemStatusFlag
    // (bit 0 = Battery Saver on). This mirrors the documented technique
    // other open-source Windows utilities use to detect Battery Saver
    // without a heavier Power Management API dependency.
    const bool battery_saver_on = (status.SystemStatusFlag & 1) != 0;
    return battery_saver_on ? platform_common::PowerState::OnBatterySaver
                              : platform_common::PowerState::OnBatteryNormal;
}

bool read_is_fullscreen_app_active() {
    // Section 3.4: pause/reduce animated wallpaper rendering when a
    // fullscreen app/game is active. SHQueryUserNotificationState is the
    // standard, well-documented technique for this on Windows (used by
    // other wallpaper engines referenced in Section 6, e.g. Lively
    // Wallpaper) - it reports QUNS_RUNNING_D3D_FULL_SCREEN /
    // QUNS_PRESENTATION_MODE among other "don't disturb" states.
    QUERY_USER_NOTIFICATION_STATE state;
    if (FAILED(SHQueryUserNotificationState(&state))) return false;
    return state == QUNS_RUNNING_D3D_FULL_SCREEN || state == QUNS_PRESENTATION_MODE ||
           state == QUNS_BUSY;
}

bool read_is_connected_to_internet() {
    // NetworkListManager COM interface - the modern, documented way to
    // query connectivity state on Windows without polling a raw socket.
    ComPtr<INetworkListManager> nlm;
    HRESULT hr = CoCreateInstance(CLSID_NetworkListManager, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&nlm));
    if (FAILED(hr) || nlm == nullptr) return true; // fail open
    VARIANT_BOOL connected = VARIANT_FALSE;
    nlm->get_IsConnectedToInternet(&connected);
    return connected == VARIANT_TRUE;
}

// Hidden message-only window used solely to receive WM_WTSSESSION_CHANGE
// (session lock/unlock - Section 3.5) via WTSRegisterSessionNotification,
// which requires a window handle. pump_events() drains this window's
// queue; the app never shows or otherwise uses this HWND.
LRESULT CALLBACK HiddenWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

class WindowsPlatformHooks : public platform_common::IPlatformHooks {
public:
    WindowsPlatformHooks()
        : last_power_state_(read_power_state()),
          last_network_up_(read_is_connected_to_internet()) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = HiddenWindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"WeatherPaperHiddenHookWindow";
        RegisterClassW(&wc);
        hwnd_ = CreateWindowW(wc.lpszClassName, L"", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
        if (hwnd_ != nullptr) {
            SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
            WTSRegisterSessionNotification(hwnd_, NOTIFY_FOR_THIS_SESSION);
        }
    }

    ~WindowsPlatformHooks() override {
        if (hwnd_ != nullptr) {
            WTSUnRegisterSessionNotification(hwnd_);
            DestroyWindow(hwnd_);
        }
    }

    void on_session_locked(VoidCallback cb) override { locked_cbs_.push_back(std::move(cb)); }
    void on_session_unlocked(VoidCallback cb) override { unlocked_cbs_.push_back(std::move(cb)); }
    void on_power_state_changed(PowerStateCallback cb) override { power_cbs_.push_back(std::move(cb)); }
    void on_network_reconnected(VoidCallback cb) override { network_cbs_.push_back(std::move(cb)); }

    platform_common::PowerState current_power_state() const override { return last_power_state_; }
    bool is_fullscreen_app_active() const override { return read_is_fullscreen_app_active(); }

    void pump_events() override {
        // Drain any pending WM_WTSSESSION_CHANGE messages for our hidden
        // window (non-blocking - PeekMessage never sleeps, matching
        // Section 2.2's "no busy-polling"; this is called from the app's
        // existing OS timer tick, not a dedicated spin loop).
        if (hwnd_ != nullptr) {
            MSG msg;
            while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessageW(&msg);
            }
        }

        auto power = read_power_state();
        if (power != last_power_state_) {
            last_power_state_ = power;
            for (auto& cb : power_cbs_) cb(power);
        }

        bool net_up = read_is_connected_to_internet();
        if (net_up && !last_network_up_) {
            for (auto& cb : network_cbs_) cb();
        }
        last_network_up_ = net_up;
    }

    // Called by HiddenWindowProc when a WM_WTSSESSION_CHANGE arrives.
    void handle_session_change(WPARAM wparam) {
        if (wparam == WTS_SESSION_LOCK) {
            for (auto& cb : locked_cbs_) cb();
        } else if (wparam == WTS_SESSION_UNLOCK) {
            for (auto& cb : unlocked_cbs_) cb();
        }
    }

private:
    HWND hwnd_ = nullptr;
    platform_common::PowerState last_power_state_;
    bool last_network_up_;
    std::vector<VoidCallback> locked_cbs_;
    std::vector<VoidCallback> unlocked_cbs_;
    std::vector<PowerStateCallback> power_cbs_;
    std::vector<VoidCallback> network_cbs_;
};

LRESULT CALLBACK HiddenWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_WTSSESSION_CHANGE) {
        auto* self = reinterpret_cast<WindowsPlatformHooks*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (self != nullptr) self->handle_session_change(wparam);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace
} // namespace weatherpaper::platform_windows

namespace weatherpaper::platform_common {
std::unique_ptr<IPlatformHooks> create_platform_hooks() {
    return std::make_unique<platform_windows::WindowsPlatformHooks>();
}
} // namespace weatherpaper::platform_common

#endif // _WIN32
