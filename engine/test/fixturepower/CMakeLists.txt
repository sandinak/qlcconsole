add_executable(fixturepower_test WIN32
    fixturepower_test.cpp fixturepower_test.h
)
target_include_directories(fixturepower_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(fixturepower_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
