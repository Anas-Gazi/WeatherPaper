add_executable(test_notify test_notify.cpp)
target_link_libraries(test_notify PRIVATE weatherpaper::notify wp_thirdparty_doctest)
add_test(NAME notify COMMAND test_notify)
