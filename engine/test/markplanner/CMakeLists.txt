add_executable(markplanner_test WIN32
    markplanner_test.cpp markplanner_test.h
)
target_include_directories(markplanner_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(markplanner_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
