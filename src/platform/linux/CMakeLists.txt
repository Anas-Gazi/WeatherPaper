# Platform backend: Linux (Section 3.6) - the highest-risk module per the
# spec. backend.cpp contains ALL decision logic (DE detection, argv command
# building) and is fully unit tested with zero OS dependency (see
# tests/platform_linux). process_runner.cpp, wallpaper_linux.cpp, and
# hooks_linux.cpp are thin OS-integration glue around that tested core.
add_library(wp_platform_linux STATIC
    src/backend.cpp
    src/process_runner.cpp
    src/wallpaper_linux.cpp
    src/hooks_linux.cpp
)

target_include_directories(wp_platform_linux
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_platform_linux
    PUBLIC
        weatherpaper::platform_common
    PRIVATE
        weatherpaper::scaling_and_fit
        weatherpaper::tag_system
)

target_compile_features(wp_platform_linux PUBLIC cxx_std_20)
set_target_properties(wp_platform_linux PROPERTIES OUTPUT_NAME "weatherpaper_platform_linux")
add_library(weatherpaper::platform_linux ALIAS wp_platform_linux)
