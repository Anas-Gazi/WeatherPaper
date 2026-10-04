# Module: render_engine - depends on platform_common (interface only, no
# concrete OS backend needed to build/test the compositor math),
# scaling_and_fit, and wallpaper_engine_core. FFmpeg is optional
# (WEATHERPAPER_WITH_FFMPEG) and only required for the animated/video tier -
# static-mode rendering and the FrameCompositor unit tests need neither
# FFmpeg nor a display server.
add_library(wp_render_engine STATIC
    src/render_engine.cpp
)

target_include_directories(wp_render_engine
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(wp_render_engine
    PUBLIC
        weatherpaper::platform_common
        weatherpaper::scaling_and_fit
        weatherpaper::wallpaper_engine_core
)

target_compile_features(wp_render_engine PUBLIC cxx_std_20)
set_target_properties(wp_render_engine PROPERTIES OUTPUT_NAME "weatherpaper_render_engine")

if(WEATHERPAPER_WITH_FFMPEG)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFMPEG QUIET libavformat libavcodec libswscale libavutil)
    endif()
    if(FFMPEG_FOUND)
        target_compile_definitions(wp_render_engine PUBLIC WEATHERPAPER_WITH_FFMPEG)
        target_include_directories(wp_render_engine PRIVATE ${FFMPEG_INCLUDE_DIRS})
        target_link_libraries(wp_render_engine PRIVATE ${FFMPEG_LIBRARIES})
        target_link_directories(wp_render_engine PRIVATE ${FFMPEG_LIBRARY_DIRS})
    else()
        message(WARNING "WEATHERPAPER_WITH_FFMPEG is ON but FFmpeg dev libraries "
                         "(libavformat/libavcodec/libswscale/libavutil) were not found via "
                         "pkg-config - building render_engine WITHOUT video decode support. "
                         "Install e.g. libavformat-dev libavcodec-dev libswscale-dev libavutil-dev "
                         "(Linux) and re-run cmake to enable animated wallpapers.")
    endif()
endif()

add_library(weatherpaper::render_engine ALIAS wp_render_engine)
