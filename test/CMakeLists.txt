add_executable(test-program-markers
               test_driver.cpp
               test_tool.cpp
               dce_marker_test.cpp
               vr_marker_test.cpp
               print_diff.cpp)

target_link_libraries(test-program-markers PRIVATE Catch2::Catch2 Markerslib)
target_include_directories(test-program-markers SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/extern)

catch_discover_tests(test-program-markers)
