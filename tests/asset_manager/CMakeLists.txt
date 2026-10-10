
find_package(OpenSSL QUIET)

add_executable(test_asset_manager test_asset_manager.cpp)
target_link_libraries(test_asset_manager PRIVATE
    weatherpaper::asset_manager
    wp_thirdparty_doctest
)

if(OpenSSL_FOUND)
    target_link_libraries(test_asset_manager PRIVATE OpenSSL::Crypto)
endif()

add_test(NAME asset_manager COMMAND test_asset_manager)