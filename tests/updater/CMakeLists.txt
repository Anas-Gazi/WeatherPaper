add_executable(test_updater test_updater.cpp)
target_link_libraries(test_updater PRIVATE weatherpaper::updater weatherpaper::asset_manager wp_thirdparty_doctest)
add_test(NAME updater COMMAND test_updater)
