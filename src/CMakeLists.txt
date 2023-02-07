add_library(Markerslib
            ASTEdits.cpp
            CommandLine.cpp
            DCEInstrumenter.cpp
            Matchers.cpp
            RangeSelectors.cpp
            ValueRangeInstrumenter.cpp
            VersionChecks.cpp)
        target_include_directories(Markerslib PUBLIC ${CLANG_INCLUDE_DIRS} ${LLVM_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR})

if(CLANG_LINK_CLANG_DYLIB)
    target_link_libraries(Markerslib PUBLIC LLVM)
    clang_target_link_libraries(Markerslib PUBLIC)
else()
    llvm_map_components_to_libnames(llvm_libs support core)
    target_link_libraries(Markerslib PUBLIC ${llvm_libs}
                                                   clangASTMatchers
                                                   clangTransformer
                                                   clangTooling)
endif(CLANG_LINK_CLANG_DYLIB)


add_subdirectory(tool)
