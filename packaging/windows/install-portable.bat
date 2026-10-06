@echo off
REM WeatherPaper Portable Windows Quick Autostart Setup
REM Run this to enable WeatherPaper to start automatically on Windows logon

setlocal
set "EXE_PATH=%~dp0weatherpaperd.exe"
if not exist "%EXE_PATH%" set "EXE_PATH=%~dp0bin\weatherpaperd.exe"

if not exist "%EXE_PATH%" (
    echo Error: weatherpaperd.exe not found in this folder!
    pause
    exit /b 1
)

echo Adding WeatherPaper to Windows startup...
reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "WeatherPaper" /t REG_SZ /d "\"%EXE_PATH%\"" /f

if %ERRORLEVEL% equ 0 (
    echo [OK] WeatherPaper will now start automatically when you log into Windows!
) else (
    echo [ERROR] Failed to add registry key.
)

pause
