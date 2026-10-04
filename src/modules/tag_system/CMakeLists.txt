# Module: tag_system - depends on scaling_and_fit (FitMode type) and the
# vendored nlohmann/json header-only library. No other dependencies.
add_library(wp_tag_system STATIC
    src/tag_system.cpp
)

target_include_directories(wp_tag_system
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_tag_system
    PUBLIC
        weatherpaper::scaling_and_fit
    PRIVATE
        wp_thirdparty_json
)

target_compile_features(wp_tag_system PUBLIC cxx_std_20)
set_target_properties(wp_tag_system PROPERTIES OUTPUT_NAME "weatherpaper_tag_system")
add_library(weatherpaper::tag_system ALIAS wp_tag_system)
