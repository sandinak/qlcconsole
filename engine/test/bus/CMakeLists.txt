add_executable(bus_test WIN32
    bus_test.cpp bus_test.h
)
target_include_directories(bus_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(bus_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
