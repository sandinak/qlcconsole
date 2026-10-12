add_executable(networktopology_test WIN32
    networktopology_test.cpp networktopology_test.h
)
target_include_directories(networktopology_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(networktopology_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
