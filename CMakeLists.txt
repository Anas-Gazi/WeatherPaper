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

# ---------------------------------------------------------------------------
# Installation and Packaging (CPack)
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

# Install bundled default theme assets
install(DIRECTORY assets/default_theme/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/weatherpaper/default_theme
    FILES_MATCHING
    PATTERN "*.json"
    PATTERN "*.jpg"
    PATTERN "*.png"
    PATTERN "*.webp"
    PATTERN "*.mp4"
    PATTERN "*.webm"
    PATTERN "README.md"
)

# Install icons in standard XDG hicolor locations
install(FILES assets/icons/weatherpaper-icon-16.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/16x16/apps
    RENAME weatherpaper.png
)
install(FILES assets/icons/weatherpaper-icon-24.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/24x24/apps
    RENAME weatherpaper.png
)
install(FILES assets/icons/weatherpaper-icon-32.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps
    RENAME weatherpaper.png
)
install(FILES assets/icons/weatherpaper-icon-256.png
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
    RENAME weatherpaper.png
)
install(FILES assets/icons/weatherpaper-icon.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    RENAME weatherpaper.svg
)
# Also copy all icons under weatherpaper/icons for runtime fallback lookup
install(DIRECTORY assets/icons/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/weatherpaper/icons
)

# Linux desktop environment integration
if(UNIX AND NOT APPLE)
    install(FILES packaging/linux/weatherpaper.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    install(FILES packaging/linux/weatherpaper-autostart.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/weatherpaper
    )
    install(FILES packaging/linux/weatherpaper.service
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/user
    )
endif()

# CPack configuration
set(CPACK_PACKAGE_NAME "weatherpaper")
set(CPACK_PACKAGE_VENDOR "WeatherPaper Contributors")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Lightweight live weather-reactive wallpaper engine")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Anas-Gazi/WeatherPaper")
set(CPACK_PACKAGE_CONTACT "Anas Gazi <contact@example.com>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/README.md")

if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;TGZ")
    set(CPACK_DEBIAN_PACKAGE_NAME "weatherpaper")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS OFF)
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libcurl4 | libcurl4t64, libssl3 | libssl3t64, libayatana-appindicator3-1, libqt6widgets6, libqt6core6, libgtk-3-0")
    set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
        "${CMAKE_SOURCE_DIR}/packaging/linux/postinst;${CMAKE_SOURCE_DIR}/packaging/linux/prerm"
    )
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "WeatherPaper")
    set(CPACK_NSIS_PACKAGE_NAME "WeatherPaper")
    set(CPACK_NSIS_MODIFY_PATH ON)

    get_filename_component(WEATHERPAPER_MINGW_BIN_DIR
        "${CMAKE_CXX_COMPILER}" DIRECTORY
    )

    foreach(WEATHERPAPER_RUNTIME_DLL
        libstdc++-6.dll
        libgcc_s_seh-1.dll
        libwinpthread-1.dll
    )
        install(FILES
            "${WEATHERPAPER_MINGW_BIN_DIR}/${WEATHERPAPER_RUNTIME_DLL}"
            DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endforeach()

    install(FILES
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libcurl.dll"
        "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin/libz.dll"
        DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()

include(CPack)


