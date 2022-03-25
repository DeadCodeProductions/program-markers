add_executable(dead-instrument DeadInstrument.cpp)
target_link_libraries(dead-instrument PUBLIC DeadInstrumentlib)

install(TARGETS dead-instrument DESTINATION bin)
