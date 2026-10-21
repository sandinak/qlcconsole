add_executable(fixturedefupdates_test WIN32
    ../common/resource_paths.h
    fixturedefupdates_test.cpp fixturedefupdates_test.h
)
target_include_directories(fixturedefupdates_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(fixturedefupdates_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
