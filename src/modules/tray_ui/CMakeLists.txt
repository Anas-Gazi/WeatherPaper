# Module: tray_ui (Section 3.11) - native tray icon only, no cross-platform
# GUI toolkit. Only built when WEATHERPAPER_BUILD_TRAY_UI is ON, since it
# pulls in GTK3 + AppIndicator3 on Linux (a real, but non-trivial, extra
# dependency) - the core wallpaper engine and its tests never require it.
add_library(wp_tray_ui STATIC
    src/tray_ui_linux.cpp
    src/tray_ui_windows.cpp
)

target_include_directories(wp_tray_ui
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_features(wp_tray_ui PUBLIC cxx_std_20)
set_target_properties(wp_tray_ui PROPERTIES OUTPUT_NAME "weatherpaper_tray_ui")

if(UNIX AND NOT APPLE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(APPINDICATOR REQUIRED ayatana-appindicator3-0.1)
    target_include_directories(wp_tray_ui PRIVATE ${APPINDICATOR_INCLUDE_DIRS})
    target_link_libraries(wp_tray_ui PRIVATE ${APPINDICATOR_LIBRARIES})
    target_link_directories(wp_tray_ui PRIVATE ${APPINDICATOR_LIBRARY_DIRS})
    target_compile_options(wp_tray_ui PRIVATE ${APPINDICATOR_CFLAGS_OTHER})
elseif(WIN32)
    target_link_libraries(wp_tray_ui PRIVATE shell32 user32)
endif()

add_library(weatherpaper::tray_ui ALIAS wp_tray_ui)
