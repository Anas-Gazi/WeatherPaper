#if defined(_WIN32)
#include "weatherpaper/notify/notify.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

namespace weatherpaper::notify {

// VERIFICATION STATUS: see platform_windows's file-header caveat - written
// against the documented Win32 API, not compiled/run on real Windows
// hardware in this drop.
//
// ASSUMPTION: uses the classic Shell_NotifyIcon balloon-tip API rather
// than the WinRT ToastNotificationManager, specifically because it needs
// no AppUserModelID/manifest registration to work from a plain unpackaged
// .exe (Section 2 "small footprint", no MSIX packaging requirement),
// trading a slightly older-looking notification style for zero extra
// packaging complexity. Upgrading to modern Action Center toasts is a
// self-contained follow-up noted in CONTRIBUTING.md.
void WindowsToastNotifier::show(const NotificationText& text) noexcept {
    // A hidden message-only window is required as the NOTIFYICONDATA
    // owner; we create one, show the balloon, and let it be cleaned up by
    // the OS at process exit rather than tracking it across calls, since
    // notifications are infrequent (severe weather only) and this keeps
    // the class stateless and noexcept-simple.
    static bool class_registered = false;
    static HWND hwnd = nullptr;
    if (!class_registered) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = DefWindowProcW;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"WeatherPaperNotifyWindow";
        RegisterClassW(&wc);
        hwnd = CreateWindowW(wc.lpszClassName, L"", 0, 0, 0, 0, 0,
                               HWND_MESSAGE, nullptr, wc.hInstance, nullptr);
        class_registered = true;
    }
    if (hwnd == nullptr) return; // fail silently - see file header contract

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(nid);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO | NIF_ICON | NIF_TIP;
    nid.dwInfoFlags = NIIF_WARNING;
    nid.hIcon = LoadIconW(nullptr, MAKEINTRESOURCEW(32516));
    wcsncpy_s(nid.szTip, L"WeatherPaper", _TRUNCATE);

    // Best-effort narrow->wide conversion; truncation on very long text is
    // acceptable for a toast body (Windows also truncates on-screen).
    MultiByteToWideChar(CP_UTF8, 0, text.title.c_str(), -1, nid.szInfoTitle, 64);
    MultiByteToWideChar(CP_UTF8, 0, text.body.c_str(), -1, nid.szInfo, 256);

    Shell_NotifyIconW(NIM_ADD, &nid);
    Shell_NotifyIconW(NIM_MODIFY, &nid); // triggers the balloon on some shell versions
    // Deliberately not calling NIM_DELETE here - the icon self-removes
    // once the balloon is dismissed/expires, and keeping the tray slot
    // around briefly is harmless for an infrequent severe-weather alert.
}

} // namespace weatherpaper::notify
#endif // _WIN32
