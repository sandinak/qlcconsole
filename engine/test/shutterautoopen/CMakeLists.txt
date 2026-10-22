add_executable(shutterautoopen_test WIN32
    ../common/resource_paths.h
    ../mastertimer/mastertimer_stub.cpp ../mastertimer/mastertimer_stub.h
    shutterautoopen_test.cpp shutterautoopen_test.h
)
target_include_directories(shutterautoopen_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
    ../mastertimer
)

target_link_libraries(shutterautoopen_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

target_compile_definitions(shutterautoopen_test PRIVATE
    SHUTTER_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}/"
)
