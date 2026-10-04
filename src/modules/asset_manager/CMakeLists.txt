# Module: asset_manager - depends on tag_system, scaling_and_fit (upload
# warning thresholds), and weather_fetch (reused IHttpClient - see header
# comment for why). Optionally depends on OpenSSL for real SHA-256/RSA
# signature verification (WEATHERPAPER_WITH_OPENSSL) - without it, checksum/
# signature functions fail closed (return false/nullopt) rather than
# silently skipping verification.
add_library(wp_asset_manager STATIC
    src/asset_manager.cpp
)

target_include_directories(wp_asset_manager
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_asset_manager
    PUBLIC
        weatherpaper::tag_system
        weatherpaper::scaling_and_fit
        weatherpaper::weather_fetch
    PRIVATE
        wp_thirdparty_json
)

target_compile_features(wp_asset_manager PUBLIC cxx_std_20)
set_target_properties(wp_asset_manager PROPERTIES OUTPUT_NAME "weatherpaper_asset_manager")

if(WEATHERPAPER_WITH_OPENSSL)
    find_package(OpenSSL QUIET)
    if(OpenSSL_FOUND)
        target_compile_definitions(wp_asset_manager PUBLIC WEATHERPAPER_WITH_OPENSSL)
        target_link_libraries(wp_asset_manager PRIVATE OpenSSL::Crypto)
    else()
        message(WARNING "WEATHERPAPER_WITH_OPENSSL is ON but OpenSSL was not found - "
                         "building asset_manager WITHOUT real checksum/signature "
                         "verification (functions will fail closed). Install "
                         "libssl-dev (Linux) and re-run cmake.")
    endif()
endif()

add_library(weatherpaper::asset_manager ALIAS wp_asset_manager)
