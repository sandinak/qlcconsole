add_executable(showtree_test WIN32
    showtree_test.cpp showtree_test.h
)
target_include_directories(showtree_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(showtree_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

target_compile_definitions(showtree_test PRIVATE
    TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../common/fixtures/"
    SAMPLE_SHOW="${CMAKE_CURRENT_SOURCE_DIR}/../../../resources/samples/Sample.qxw"
)
