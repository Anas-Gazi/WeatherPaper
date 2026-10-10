# The weatherpaperd binary - orchestrates every module (see main.cpp's file
# header for why this is the one place allowed to depend on "everything").
add_executable(weatherpaperd main.cpp)

target_link_libraries(weatherpaperd
    PRIVATE
        weatherpaper::config
        weatherpaper::tag_system
        weatherpaper::wallpaper_engine_core
        weatherpaper::time_of_day
        weatherpaper::weather_fetch
        weatherpaper::platform_common
        weatherpaper::render_engine
        weatherpaper::asset_manager
        weatherpaper::notify
)

# Optional UI modules - only link if they were actually built.
if(WEATHERPAPER_BUILD_TRAY_UI)
    target_link_libraries(weatherpaperd PRIVATE weatherpaper::tray_ui)
endif()
if(WEATHERPAPER_BUILD_SETTINGS_UI)
    target_link_libraries(weatherpaperd PRIVATE weatherpaper::settings_ui)
endif()

if(WIN32)
    target_link_libraries(weatherpaperd PRIVATE weatherpaper::platform_windows)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(weatherpaperd PRIVATE weatherpaper::platform_linux)
endif()

# Dev/sandbox convenience: point straight at the in-tree bundled theme so
# `weatherpaperd --once` works from a build directory without an install
# step. Packaging (Section 7) should override this to the installed
# location (see docs/CONTRIBUTING.md "Packaging").
target_compile_definitions(weatherpaperd PRIVATE
    WEATHERPAPER_BUNDLED_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets/default_theme"
)

target_compile_features(weatherpaperd PRIVATE cxx_std_20)

include(GNUInstallDirs)
install(TARGETS weatherpaperd
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)



if(WIN32)
    enable_language(RC)
    configure_file(
        "${CMAKE_SOURCE_DIR}/packaging/windows/weatherpaper.rc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/weatherpaper.rc"
        @ONLY
    )

    add_executable(WeatherPaper WIN32
        "${CMAKE_SOURCE_DIR}/packaging/windows/weatherpaper_launcher.cpp"
        "${CMAKE_CURRENT_BINARY_DIR}/weatherpaper.rc"
    )

    install(TARGETS WeatherPaper
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endif()


