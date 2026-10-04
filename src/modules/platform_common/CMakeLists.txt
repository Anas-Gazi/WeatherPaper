# Module: platform_common - INTERFACE ONLY. No .cpp file: this library
# exists purely to publish the shared header that platform_windows and
# platform_linux both implement against. Because it's header-only with no
# translation unit, it's declared as a CMake INTERFACE library (no object
# code to compile/link here at all).
add_library(wp_platform_common INTERFACE)

target_include_directories(wp_platform_common
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_platform_common
    INTERFACE
        weatherpaper::scaling_and_fit
        weatherpaper::tag_system
)

target_compile_features(wp_platform_common INTERFACE cxx_std_20)
add_library(weatherpaper::platform_common ALIAS wp_platform_common)
