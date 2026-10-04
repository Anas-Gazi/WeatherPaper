# Only builds on Linux (see top-level tests/CMakeLists.txt guard).
# Links against platform_linux's backend logic only - never spawns a real
# process or touches a real display server, per the module's testability
# design (see backend.hpp header comment).
add_executable(test_platform_linux_backend test_platform_linux_backend.cpp)
target_link_libraries(test_platform_linux_backend PRIVATE weatherpaper::platform_linux wp_thirdparty_doctest)
add_test(NAME platform_linux_backend COMMAND test_platform_linux_backend)
