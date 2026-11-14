add_executable(candlepresses_test WIN32
    candlepresses_test.cpp candlepresses_test.h
)
target_include_directories(candlepresses_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(candlepresses_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
