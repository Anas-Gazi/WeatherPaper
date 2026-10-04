# Platform backend: Windows (Section 3.5). Only added to the build graph
# when CMAKE_SYSTEM_NAME is Windows (see top-level CMakeLists.txt's
# if(WIN32) guard) - this file is never evaluated on a Linux build, so it's
# safe for it to assume MSVC/MinGW + Windows SDK headers are available.
#
# VERIFICATION STATUS: this target has been written against the documented
# Win32/COM API surface but has not been compiled on real Windows hardware
# as part of producing this repository - see docs/ARCHITECTURE.md
# "Verification status". A Windows contributor's first useful PR is often
# simply "fixed the build" here; please report/fix any compile errors you
# hit rather than assuming they're intentional.
add_library(wp_platform_windows STATIC
    src/wallpaper_windows.cpp
    src/hooks_windows.cpp
)

target_include_directories(wp_platform_windows
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_platform_windows
    PUBLIC
        weatherpaper::platform_common
    PRIVATE
        weatherpaper::scaling_and_fit
        weatherpaper::tag_system
)

if(WIN32)
    target_link_libraries(wp_platform_windows PRIVATE
        ole32       # COM (CoCreateInstance, IDesktopWallpaper, NetworkListManager)
        shell32     # SHQueryUserNotificationState
        wtsapi32    # WTSRegisterSessionNotification
        propsys
    )
endif()

target_compile_features(wp_platform_windows PUBLIC cxx_std_20)
set_target_properties(wp_platform_windows PROPERTIES OUTPUT_NAME "weatherpaper_platform_windows")
add_library(weatherpaper::platform_windows ALIAS wp_platform_windows)
