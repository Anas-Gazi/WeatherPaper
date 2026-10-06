@echo off
REM WeatherPaper Portable Windows Autostart Removal

setlocal
echo Removing WeatherPaper from Windows startup...
reg delete "HKCU\Software\Microsoft\Windows\CurrentVersion\Run" /v "WeatherPaper" /f 2>nul

echo [OK] Removed from Windows startup.
pause
