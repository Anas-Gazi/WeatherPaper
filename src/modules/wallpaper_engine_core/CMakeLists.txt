# Module: wallpaper_engine_core - depends on time_of_day and tag_system
# (both pure/local modules). Deliberately does NOT depend on weather_fetch -
# see the header comment for why that dependency direction matters.
add_library(wp_wallpaper_engine_core STATIC
    src/wallpaper_engine_core.cpp
)

target_include_directories(wp_wallpaper_engine_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_wallpaper_engine_core
    PUBLIC
        weatherpaper::time_of_day
        weatherpaper::tag_system
)

target_compile_features(wp_wallpaper_engine_core PUBLIC cxx_std_20)
set_target_properties(wp_wallpaper_engine_core PROPERTIES OUTPUT_NAME "weatherpaper_wallpaper_engine_core")
add_library(weatherpaper::wallpaper_engine_core ALIAS wp_wallpaper_engine_core)
