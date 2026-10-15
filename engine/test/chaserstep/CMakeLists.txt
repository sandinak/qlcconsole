add_executable(chaserstep_test WIN32
    chaserstep_test.cpp chaserstep_test.h
)
target_include_directories(chaserstep_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(chaserstep_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
