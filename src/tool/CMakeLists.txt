add_executable(program-markers ProgramMarkers.cpp)
target_link_libraries(program-markers PUBLIC Markerslib)

install(TARGETS program-markers DESTINATION bin)
