add_executable(callingscript_test WIN32
    callingscript_test.cpp callingscript_test.h
)
target_include_directories(callingscript_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(callingscript_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

# script-excerpt.txt: the format of a real script, for the Cues from the
# script tests.
target_compile_definitions(callingscript_test PRIVATE
    CALLINGSCRIPT_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
