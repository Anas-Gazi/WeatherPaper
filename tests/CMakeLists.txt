# Each subdirectory here is a standalone test binary for one module, using
# the vendored doctest single-header framework (third_party/doctest).
# Run everything with: ctest --test-dir build --output-on-failure

add_subdirectory(time_of_day)
add_subdirectory(scaling_and_fit)
add_subdirectory(tag_system)
add_subdirectory(wallpaper_engine_core)
add_subdirectory(config)
add_subdirectory(render_engine)
add_subdirectory(weather_fetch)
add_subdirectory(asset_manager)
add_subdirectory(updater)
add_subdirectory(notify)

if(UNIX AND NOT APPLE)
    add_subdirectory(platform_linux)
endif()
