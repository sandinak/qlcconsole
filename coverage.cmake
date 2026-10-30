if(coverage)
    # -fprofile-update=atomic: the engine writes DMX on its own threads (the
    # MasterTimer, each Universe), and plain counter updates raced -- lcov
    # read negative hit counts for genericfader.cpp, universe.cpp and the
    # monitor rasteriser, and dropped those lines.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage -fprofile-update=atomic")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fprofile-arcs -ftest-coverage -fprofile-update=atomic")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lgcov")
    # Tests that measure how long drawing takes know their numbers mean
    # nothing here: every line counts itself (Performers_Test render cost).
    add_compile_definitions(QLC_INSTRUMENTED_BUILD)
    file(GLOB_RECURSE gcov_files "${CMAKE_BINARY_DIR}/*.gcno" "${CMAKE_BINARY_DIR}/*.gcda")
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES ${gcov_files})
endif()
