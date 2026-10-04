# Module: time_of_day
# Pure logic library. No dependencies on any other WeatherPaper module,
# no OS libraries, no network. This is intentional - it is the first module
# built and the easiest to keep correct (see docs/ARCHITECTURE.md).

add_library(wp_time_of_day STATIC
    src/time_of_day.cpp
)

target_include_directories(wp_time_of_day
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(wp_time_of_day PUBLIC cxx_std_20)

set_target_properties(wp_time_of_day PROPERTIES
    OUTPUT_NAME "weatherpaper_time_of_day"
)

add_library(weatherpaper::time_of_day ALIAS wp_time_of_day)
