add_executable(fixture_test WIN32
    ../common/resource_paths.h
    fixture_test.cpp fixture_test.h
)
target_include_directories(fixture_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(fixture_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
