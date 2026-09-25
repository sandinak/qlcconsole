add_executable(networkreport_test WIN32
    networkreport_test.cpp networkreport_test.h
)
target_include_directories(networkreport_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(networkreport_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
