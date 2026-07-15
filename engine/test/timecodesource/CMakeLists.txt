add_executable(timecodesource_test WIN32
    timecodesource_test.cpp timecodesource_test.h
)
target_include_directories(timecodesource_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(timecodesource_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
