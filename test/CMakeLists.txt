add_executable(dead_instrument_test test_driver.cpp test_tool.cpp marker_test.cpp static_global_test.cpp)

target_link_libraries(dead_instrument_test PRIVATE Catch2::Catch2 DeadInstrumentlib)
target_include_directories(dead_instrument_test SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/extern)

catch_discover_tests(dead_instrument_test)
