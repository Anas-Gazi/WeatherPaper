#include <windows.h>
#include <string>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    wchar_t module[MAX_PATH];
    DWORD length = GetModuleFileNameW(nullptr, module, MAX_PATH);

    if (length == 0 || length >= MAX_PATH) {
        MessageBoxW(nullptr, L"Could not locate the WeatherPaper launcher.",
                    L"WeatherPaper", MB_OK | MB_ICONERROR);
        return 1;
    }

    std::wstring folder(module, length);
    const auto slash = folder.find_last_of(L"\\/");
    if (slash == std::wstring::npos) return 1;

    folder.resize(slash);
    const std::wstring exe = folder + L"\\weatherpaperd.exe";
    std::wstring command = L"\"" + exe + L"\"";

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};

    if (!CreateProcessW(
            exe.c_str(), command.data(), nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, folder.c_str(),
            &startup, &process)) {
        MessageBoxW(nullptr,
                    L"WeatherPaper could not start. Check that its files are present.",
                    L"WeatherPaper", MB_OK | MB_ICONERROR);
        return 1;
    }

    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}
