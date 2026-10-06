@echo off
REM WeatherPaper Windows Release Builder & Packager
REM Tested on Windows 10 and Windows 11 with Visual Studio 2022 or MinGW-w64

setlocal enabledelayedexpansion
set "SCRIPT_DIR=%~dp0"
set "ROOT_DIR=%SCRIPT_DIR%..\.."
set "BUILD_DIR=%ROOT_DIR%\build-windows"
set "DIST_DIR=%ROOT_DIR%\dist"

if not exist "%DIST_DIR%" mkdir "%DIST_DIR%"

echo ========================================================
echo   WeatherPaper Windows 10/11 Builder & Packager
echo ========================================================

REM 1. Configure CMake Release build
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -DWEATHERPAPER_BUILD_SETTINGS_UI=ON ^
    -DWEATHERPAPER_BUILD_TRAY_UI=ON ^
    -DWEATHERPAPER_BUILD_TESTS=OFF

if %ERRORLEVEL% neq 0 (
    echo Error during CMake configuration!
    exit /b %ERRORLEVEL%
)

REM 2. Compile Release
cmake --build "%BUILD_DIR%" --config Release --parallel

if %ERRORLEVEL% neq 0 (
    echo Error during build!
    exit /b %ERRORLEVEL%
)

REM 3. If Qt6 is present, run windeployqt to collect runtime DLLs
set "EXE_PATH=%BUILD_DIR%\src\app\Release\weatherpaperd.exe"
if not exist "%EXE_PATH%" set "EXE_PATH=%BUILD_DIR%\src\app\weatherpaperd.exe"

where windeployqt >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo Running windeployqt to package Qt dependencies...
    windeployqt --release --no-translations "%EXE_PATH%"
)

REM 4. If Inno Setup compiler (ISCC.exe) is installed, build the Windows Setup executable
set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=C:\Program Files\Inno Setup 6\ISCC.exe"

where ISCC >nul 2>nul
if %ERRORLEVEL% equ 0 set "ISCC=ISCC"

if exist "%ISCC%" (
    echo Compiling Windows Installer with Inno Setup...
    "%ISCC%" "%SCRIPT_DIR%weatherpaper.iss"
    echo Installer built in %DIST_DIR%!
) else (
    echo Inno Setup not found. Generating portable ZIP package instead...
    cmake --install "%BUILD_DIR%" --prefix "%BUILD_DIR%\install"
    powershell Compress-Archive -Path "%BUILD_DIR%\install\*" -DestinationPath "%DIST_DIR%\WeatherPaper-Windows-Portable.zip" -Force
    echo Portable ZIP created at %DIST_DIR%\WeatherPaper-Windows-Portable.zip
)

echo Build and packaging finished successfully!
