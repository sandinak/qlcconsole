add_executable(markplanner_test WIN32
    markplanner_test.cpp markplanner_test.h
)
target_include_directories(markplanner_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(markplanner_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
