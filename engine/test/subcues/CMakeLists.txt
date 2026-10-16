add_executable(subcues_test WIN32
    subcues_test.cpp subcues_test.h
)
target_include_directories(subcues_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(subcues_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
