add_executable(monitorproperties_test WIN32
    monitorproperties_test.cpp monitorproperties_test.h
)
target_include_directories(monitorproperties_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(monitorproperties_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
