add_executable(qlcpoint_test WIN32
    qlcpoint_test.cpp qlcpoint_test.h
)
target_include_directories(qlcpoint_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcpoint_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
