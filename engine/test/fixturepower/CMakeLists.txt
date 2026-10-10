add_executable(fixturepower_test WIN32
    fixturepower_test.cpp fixturepower_test.h
)
target_include_directories(fixturepower_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(fixturepower_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
