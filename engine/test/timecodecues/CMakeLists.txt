add_executable(timecodecues_test WIN32
    timecodecues_test.cpp timecodecues_test.h
)
target_include_directories(timecodecues_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(timecodecues_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
