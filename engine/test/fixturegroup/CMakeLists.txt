add_executable(fixturegroup_test WIN32
    ../common/resource_paths.h
    fixturegroup_test.cpp fixturegroup_test.h
)
target_include_directories(fixturegroup_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(fixturegroup_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
