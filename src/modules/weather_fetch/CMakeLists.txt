# Module: weather_fetch - depends on wallpaper_engine_core (Condition type)
# and nlohmann/json. Optionally depends on libcurl for the production HTTP
# transport (WEATHERPAPER_WITH_CURL) - tests never need curl since they
# inject a MockHttpClient (see tests/weather_fetch).
add_library(wp_weather_fetch STATIC
    src/weather_fetch.cpp
)

target_include_directories(wp_weather_fetch
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_weather_fetch
    PUBLIC
        weatherpaper::wallpaper_engine_core
    PRIVATE
        wp_thirdparty_json
)

target_compile_features(wp_weather_fetch PUBLIC cxx_std_20)
set_target_properties(wp_weather_fetch PROPERTIES OUTPUT_NAME "weatherpaper_weather_fetch")

if(WEATHERPAPER_WITH_CURL)
    find_package(CURL QUIET)
    if(CURL_FOUND)
        target_compile_definitions(wp_weather_fetch PUBLIC WEATHERPAPER_WITH_CURL)
        target_link_libraries(wp_weather_fetch PRIVATE CURL::libcurl)
    else()
        message(WARNING "WEATHERPAPER_WITH_CURL is ON but libcurl was not found - "
                         "building weather_fetch WITHOUT the production HTTP client. "
                         "Install libcurl4-openssl-dev (Linux) or vcpkg curl (Windows) "
                         "and re-run cmake to enable live weather fetching.")
    endif()
endif()

add_library(weatherpaper::weather_fetch ALIAS wp_weather_fetch)
