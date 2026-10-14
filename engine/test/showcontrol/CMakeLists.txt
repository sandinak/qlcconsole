add_executable(showcontrol_test WIN32
    showcontrol_test.cpp showcontrol_test.h
)
target_include_directories(showcontrol_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(showcontrol_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
