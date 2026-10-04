# Module: updater - depends on weather_fetch (reused IHttpClient) and
# asset_manager (reused checksum/signature verification - Section 3.10's
# explicit "do not build two separate download/verify code paths"
# requirement).
add_library(wp_updater STATIC
    src/updater.cpp
)

target_include_directories(wp_updater
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_updater
    PUBLIC
        weatherpaper::weather_fetch
        weatherpaper::asset_manager
    PRIVATE
        wp_thirdparty_json
)

target_compile_features(wp_updater PUBLIC cxx_std_20)
set_target_properties(wp_updater PROPERTIES OUTPUT_NAME "weatherpaper_updater")
add_library(weatherpaper::updater ALIAS wp_updater)
