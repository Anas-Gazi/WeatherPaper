# Module: notify - depends on wallpaper_engine_core (Condition type for
# formatting). Platform-conditional .cpp files are compiled in based on
# the target OS; only one of notify_linux.cpp/notify_windows.cpp is ever
# active per build (each is wrapped in its own #if defined(...) guard so
# including both unconditionally in the source list is harmless).
add_library(wp_notify STATIC
    src/notify.cpp
    src/notify_linux.cpp
    src/notify_windows.cpp
)

target_include_directories(wp_notify
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_notify
    PUBLIC
        weatherpaper::wallpaper_engine_core
)

target_compile_features(wp_notify PUBLIC cxx_std_20)
set_target_properties(wp_notify PROPERTIES OUTPUT_NAME "weatherpaper_notify")
add_library(weatherpaper::notify ALIAS wp_notify)
