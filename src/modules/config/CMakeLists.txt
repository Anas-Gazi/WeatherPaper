# Module: config - depends on tag_system (SelectionPolicy type) and
# nlohmann/json. Uses Win32 SHGetKnownFolderPath on Windows, getenv-based
# XDG resolution on Linux - both branches compiled conditionally so this
# module builds cleanly on either OS without preprocessor leakage into
# callers (callers just call default_config_dir() etc.).
add_library(wp_config STATIC
    src/config.cpp
)

target_include_directories(wp_config
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_config
    PUBLIC
        weatherpaper::tag_system
    PRIVATE
        wp_thirdparty_json
)

if(WIN32)
    target_link_libraries(wp_config PRIVATE shell32 ole32)
endif()

target_compile_features(wp_config PUBLIC cxx_std_20)
set_target_properties(wp_config PROPERTIES OUTPUT_NAME "weatherpaper_config")
add_library(weatherpaper::config ALIAS wp_config)
