#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RELEASE_ZIP_URL "https://github.com/Anas-Gazi/WeatherPaper/releases/download/v0.1.0/WeatherPaper-v1.0.0-windows-x64.zip"

int main() {
    // Set console title
    SetConsoleTitleA("WeatherPaper Windows Setup (v1.0.0)");

    printf("\n");
    printf(" ========================================================\n");
    printf("       WeatherPaper Installer for Windows 10 & 11\n");
    printf(" ========================================================\n\n");

    // 1. Determine destination folder: %LOCALAPPDATA%\WeatherPaper
    char localAppData[MAX_PATH];
    if (!GetEnvironmentVariableA("LOCALAPPDATA", localAppData, MAX_PATH)) {
        strcpy(localAppData, "C:\\WeatherPaper");
    }

    char installDir[MAX_PATH];
    snprintf(installDir, sizeof(installDir), "%s\\WeatherPaper", localAppData);

    char zipPath[MAX_PATH];
    snprintf(zipPath, sizeof(zipPath), "%s\\weatherpaper_bundle.zip", localAppData);

    printf(" [*] Target install directory: %s\n\n", installDir);

    // Create target directory if needed
    CreateDirectoryA(installDir, NULL);

    // 2. Download package using Windows PowerShell
    printf(" [1/4] Downloading WeatherPaper files from GitHub...\n");
    char cmdDownload[2048];
    snprintf(cmdDownload, sizeof(cmdDownload),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; "
        "Write-Host 'Connecting to GitHub...'; "
        "$ProgressPreference = 'SilentlyContinue'; "
        "Invoke-WebRequest -Uri '%s' -OutFile '%s'\"",
        RELEASE_ZIP_URL, zipPath);

    int res = system(cmdDownload);
    if (res != 0) {
        // Fallback: try curl.exe (built-in on Windows 10/11)
        printf("       Retrying download with curl...\n");
        char cmdCurl[2048];
        snprintf(cmdCurl, sizeof(cmdCurl), "curl.exe -fSL \"%s\" -o \"%s\"", RELEASE_ZIP_URL, zipPath);
        res = system(cmdCurl);
    }

    if (res != 0) {
        printf("\n [!] Error: Failed to download WeatherPaper package.\n");
        printf("     Please check your internet connection and try again.\n");
        printf("\n Press Enter to exit...");
        getchar();
        return 1;
    }
    printf("       Download completed successfully!\n\n");

    // 3. Extract package to install directory
    printf(" [2/4] Extracting package files...\n");
    char cmdExtract[2048];
    snprintf(cmdExtract, sizeof(cmdExtract),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "
        "\"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"",
        zipPath, installDir);

    res = system(cmdExtract);
    if (res != 0) {
        // Fallback: try tar -xf (built-in on Windows 10/11)
        char cmdTar[2048];
        snprintf(cmdTar, sizeof(cmdTar), "tar.exe -xf \"%s\" -C \"%s\"", zipPath, installDir);
        res = system(cmdTar);
    }

    // Clean up temporary zip
    DeleteFileA(zipPath);
    printf("       Extraction complete!\n\n");

    // 4. Create Desktop & Start Menu Shortcuts via PowerShell
    printf(" [3/4] Creating application shortcuts...\n");
    char cmdShortcuts[4096];
    snprintf(cmdShortcuts, sizeof(cmdShortcuts),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \""
        "$ws = New-Object -ComObject WScript.Shell; "
        "$desktop = [Environment]::GetFolderPath('Desktop'); "
        "$startMenu = [Environment]::GetFolderPath('Programs'); "
        "$exePath = '%s\\weatherpaperd.exe'; "
        "$iconPath = '%s\\assets\\icons\\weatherpaper-icon-256.png'; "
        "if (-not (Test-Path $exePath)) { $exePath = '%s\\install-portable.bat' }; "
        "$s1 = $ws.CreateShortcut(\\\"$desktop\\WeatherPaper.lnk\\\"); "
        "$s1.TargetPath = $exePath; $s1.WorkingDirectory = '%s'; $s1.Save(); "
        "$s2 = $ws.CreateShortcut(\\\"$startMenu\\WeatherPaper.lnk\\\"); "
        "$s2.TargetPath = $exePath; $s2.WorkingDirectory = '%s'; $s2.Save();"
        "\"",
        installDir, installDir, installDir, installDir, installDir);

    system(cmdShortcuts);
    printf("       Shortcuts added to Desktop and Start Menu!\n\n");

    // 5. Register Autostart in Windows Registry (HKCU\Software\Microsoft\Windows\CurrentVersion\Run)
    printf(" [4/4] Setting up automatic startup on Windows boot...\n");
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        char exeRunPath[MAX_PATH];
        snprintf(exeRunPath, sizeof(exeRunPath), "\"%s\\weatherpaperd.exe\"", installDir);
        RegSetValueExA(hKey, "WeatherPaper", 0, REG_SZ, (const BYTE*)exeRunPath, (DWORD)(strlen(exeRunPath) + 1));
        RegCloseKey(hKey);
        printf("       Autostart enabled in Windows Registry!\n\n");
    }

    // Run portable startup script to ensure everything is initialized
    char cmdInit[MAX_PATH + 32];
    snprintf(cmdInit, sizeof(cmdInit), "call \"%s\\install-portable.bat\" >nul 2>&1", installDir);
    system(cmdInit);

    printf(" ========================================================\n");
    printf("    Installation Successful!\n");
    printf("    WeatherPaper is now installed and configured.\n");
    printf(" ========================================================\n\n");

    // Optional message box
    MessageBoxA(NULL,
        "WeatherPaper has been installed successfully!\n\n"
        "It will now run in your system tray and update your wallpaper based on live weather.",
        "WeatherPaper Setup",
        MB_OK | MB_ICONINFORMATION);

    return 0;
}
