add_executable(qlcpalette_test WIN32
    qlcpalette_test.cpp qlcpalette_test.h
)
target_include_directories(qlcpalette_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcpalette_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

target_compile_definitions(qlcpalette_test PRIVATE
    TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../common/fixtures/"
)
