add_executable(blocking_test WIN32
    blocking_test.cpp blocking_test.h
)
target_include_directories(blocking_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(blocking_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
