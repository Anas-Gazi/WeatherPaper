cmake_minimum_required(VERSION 3.20)
project(WeatherPaper
    VERSION 0.1.0
    DESCRIPTION "Cross-platform live weather-reactive wallpaper engine"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# WeatherPaper top-level build
#
# This file only wires modules together. Each module under src/modules/<name>
# owns its own CMakeLists.txt and is buildable/testable in isolation - that
# is the whole point of the modular architecture (see ARCHITECTURE.md).
#
# Independent internal versioning (see docs/ARCHITECTURE.md "Versioning"):
#   core (this engine)   -> WEATHERPAPER_CORE_VERSION
#   bundled asset catalog -> assets/default_theme/catalog.json "version"
#   settings UI           -> src/modules/settings_ui/VERSION
# These are deliberately NOT tied to one single project version number.
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(WEATHERPAPER_CORE_VERSION "1.0.0")

# ---------------------------------------------------------------------------
# Build options
# ---------------------------------------------------------------------------
option(WEATHERPAPER_BUILD_TESTS        "Build unit tests"                          ON)
option(WEATHERPAPER_BUILD_APP          "Build the weatherpaperd daemon/app binary" ON)
option(WEATHERPAPER_BUILD_TRAY_UI      "Build the system tray module"              OFF)
option(WEATHERPAPER_BUILD_SETTINGS_UI  "Build the Qt Widgets settings/gallery UI"  OFF)
option(WEATHERPAPER_WITH_CURL          "Build weather_fetch/updater with real libcurl HTTP client" ON)
option(WEATHERPAPER_WITH_OPENSSL       "Build checksum/signature verification with OpenSSL"        ON)
option(WEATHERPAPER_WITH_FFMPEG        "Build render_engine video decode path with FFmpeg (opt-in animated wallpapers)" OFF)

message(STATUS "WeatherPaper core version:      ${WEATHERPAPER_CORE_VERSION}")
message(STATUS "Target OS:                      ${CMAKE_SYSTEM_NAME}")
message(STATUS "WEATHERPAPER_BUILD_TESTS:       ${WEATHERPAPER_BUILD_TESTS}")
message(STATUS "WEATHERPAPER_BUILD_APP:         ${WEATHERPAPER_BUILD_APP}")
message(STATUS "WEATHERPAPER_BUILD_TRAY_UI:     ${WEATHERPAPER_BUILD_TRAY_UI}")
message(STATUS "WEATHERPAPER_BUILD_SETTINGS_UI: ${WEATHERPAPER_BUILD_SETTINGS_UI}")
message(STATUS "WEATHERPAPER_WITH_CURL:         ${WEATHERPAPER_WITH_CURL}")
message(STATUS "WEATHERPAPER_WITH_OPENSSL:      ${WEATHERPAPER_WITH_OPENSSL}")
message(STATUS "WEATHERPAPER_WITH_FFMPEG:       ${WEATHERPAPER_WITH_FFMPEG}")

# Header-only third-party libs are vendored (no network access needed to build).
add_library(wp_thirdparty_doctest INTERFACE)
target_include_directories(wp_thirdparty_doctest INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

add_library(wp_thirdparty_json INTERFACE)
target_include_directories(wp_thirdparty_json INTERFACE ${CMAKE_SOURCE_DIR}/third_party)

# ---------------------------------------------------------------------------
# Dependency-ordered module build (mirrors Section 7 of the spec):
#   1. Pure logic, no OS/network:  time_of_day, scaling_and_fit, tag_system,
#      wallpaper_engine_core, config
#   2. Network:                    weather_fetch
#   3. Platform wallpaper-setting: platform_common, platform_windows/linux
#   4. Rendering:                  render_engine
#   5. Assets & updates:           asset_manager, updater
#   6. UI:                         tray_ui, settings_ui, notify
# ---------------------------------------------------------------------------
add_subdirectory(src/modules/time_of_day)
add_subdirectory(src/modules/scaling_and_fit)
add_subdirectory(src/modules/tag_system)
add_subdirectory(src/modules/wallpaper_engine_core)
add_subdirectory(src/modules/config)

add_subdirectory(src/modules/weather_fetch)

add_subdirectory(src/modules/platform_common)
if(WIN32)
    add_subdirectory(src/platform/windows)
elseif(UNIX AND NOT APPLE)
    add_subdirectory(src/platform/linux)
endif()

add_subdirectory(src/modules/render_engine)
add_subdirectory(src/modules/asset_manager)
add_subdirectory(src/modules/updater)
add_subdirectory(src/modules/notify)

if(WEATHERPAPER_BUILD_TRAY_UI)
    add_subdirectory(src/modules/tray_ui)
    add_compile_definitions(WEATHERPAPER_BUILD_TRAY_UI)
endif()
if(WEATHERPAPER_BUILD_SETTINGS_UI)
    add_subdirectory(src/modules/settings_ui)
    add_compile_definitions(WEATHERPAPER_BUILD_SETTINGS_UI)
endif()

add_subdirectory(src/app)

if(WEATHERPAPER_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
