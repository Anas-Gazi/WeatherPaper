@echo off
setlocal
set "APP_DIR=%~dp0"
set "LAUNCHER=%APP_DIR%WeatherPaper.exe"

if not exist "%LAUNCHER%" (
    echo Error: WeatherPaper.exe was not found.
    echo Extract the complete ZIP before running this script.
    pause
    exit /b 1
)

reg add "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "WeatherPaper" /t REG_SZ /d "\"%LAUNCHER%\"" /f

if errorlevel 1 (
    echo Failed to configure automatic startup.
) else (
    echo WeatherPaper will start automatically when you log in.
)
pause
