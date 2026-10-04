#if defined(_WIN32)
#include "weatherpaper/tray_ui/tray_ui.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

// VERIFICATION STATUS: written against the documented Win32 Shell_NotifyIcon
// API (Section 3.11: "Win32 Shell_NotifyIcon on Windows"), not compiled/run
// on real Windows hardware in this drop - see docs/ARCHITECTURE.md.
namespace weatherpaper::tray_ui {
namespace {

constexpr UINT kTrayCallbackMsg = WM_APP + 1;
enum MenuCommandId : UINT {
    kCmdPauseResume = 1001,
    kCmdRefreshNow = 1002,
    kCmdSettings = 1003,
    kCmdGallery = 1004,
    kCmdQuit = 1005,
};

LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);

class WindowsTrayIcon : public ITrayIcon {
public:
    ~WindowsTrayIcon() override { destroy(); }

    bool create(const TrayCallbacks& callbacks, const std::string& icon_path) override {
        callbacks_ = callbacks;

        WNDCLASSW wc{};
        wc.lpfnWndProc = TrayWindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"WeatherPaperTrayWindow";
        RegisterClassW(&wc);
        hwnd_ = CreateWindowW(wc.lpszClassName, L"", 0, 0, 0, 0, 0,
                                HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
        if (hwnd_ == nullptr) return false;
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

        nid_ = {};
        nid_.cbSize = sizeof(nid_);
        nid_.hWnd = hwnd_;
        nid_.uID = 1;
        nid_.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
        nid_.uCallbackMessage = kTrayCallbackMsg;
        int wlen = MultiByteToWideChar(CP_UTF8, 0, icon_path.c_str(), -1, nullptr, 0);
        std::wstring wpath(static_cast<size_t>(wlen), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, icon_path.c_str(), -1, wpath.data(), wlen);
        nid_.hIcon = static_cast<HICON>(LoadImageW(nullptr, wpath.c_str(), IMAGE_ICON, 16, 16,
                                                      LR_LOADFROMFILE));
        if (nid_.hIcon == nullptr) nid_.hIcon = LoadIconW(nullptr, IDI_APPLICATION); // graceful fallback
        wcsncpy_s(nid_.szTip, L"WeatherPaper", _TRUNCATE);

        return Shell_NotifyIconW(NIM_ADD, &nid_) == TRUE;
    }

    void set_paused_label(bool is_paused) override { is_paused_ = is_paused; }

    void pump_events() override {
        MSG msg;
        while (PeekMessageW(&msg, hwnd_, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    void destroy() override {
        if (hwnd_ != nullptr) {
            Shell_NotifyIconW(NIM_DELETE, &nid_);
            DestroyWindow(hwnd_);
            hwnd_ = nullptr;
        }
    }

    void show_context_menu() {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, kCmdPauseResume,
                     is_paused_ ? L"Resume auto-updates" : L"Pause auto-updates");
        AppendMenuW(menu, MF_STRING, kCmdRefreshNow, L"Refresh Now");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kCmdSettings, L"Open Settings...");
        AppendMenuW(menu, MF_STRING, kCmdGallery, L"Open Gallery...");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, kCmdQuit, L"Quit WeatherPaper");

        POINT pt;
        GetCursorPos(&pt);
        // Required so the menu closes properly when it loses focus - a
        // well-documented Win32 tray-icon idiom.
        SetForegroundWindow(hwnd_);
        TrackPopupMenu(menu, TPM_BOTTOMALIGN | TPM_LEFTALIGN, pt.x, pt.y, 0, hwnd_, nullptr);
        DestroyMenu(menu);
    }

    void handle_command(UINT id) {
        switch (id) {
            case kCmdPauseResume: if (callbacks_.on_toggle_pause_resume) callbacks_.on_toggle_pause_resume(); break;
            case kCmdRefreshNow:  if (callbacks_.on_force_refresh_now) callbacks_.on_force_refresh_now(); break;
            case kCmdSettings:    if (callbacks_.on_open_settings) callbacks_.on_open_settings(); break;
            case kCmdGallery:     if (callbacks_.on_open_gallery) callbacks_.on_open_gallery(); break;
            case kCmdQuit:        if (callbacks_.on_quit) callbacks_.on_quit(); break;
            default: break;
        }
    }

private:
    HWND hwnd_ = nullptr;
    NOTIFYICONDATAW nid_{};
    TrayCallbacks callbacks_;
    bool is_paused_ = false;
};

LRESULT CALLBACK TrayWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    auto* self = reinterpret_cast<WindowsTrayIcon*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (self != nullptr) {
        if (msg == kTrayCallbackMsg && (lparam == WM_RBUTTONUP || lparam == WM_LBUTTONUP)) {
            self->show_context_menu();
            return 0;
        }
        if (msg == WM_COMMAND) {
            self->handle_command(LOWORD(wparam));
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

std::unique_ptr<ITrayIcon> create_platform_tray_icon() {
    return std::make_unique<WindowsTrayIcon>();
}

} // namespace weatherpaper::tray_ui
#endif // _WIN32
