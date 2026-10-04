# Module: scaling_and_fit - pure geometry logic, no dependencies.
add_library(wp_scaling_and_fit STATIC
    src/scaling_and_fit.cpp
)

target_include_directories(wp_scaling_and_fit
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(wp_scaling_and_fit PUBLIC cxx_std_20)
set_target_properties(wp_scaling_and_fit PROPERTIES OUTPUT_NAME "weatherpaper_scaling_and_fit")
add_library(weatherpaper::scaling_and_fit ALIAS wp_scaling_and_fit)
