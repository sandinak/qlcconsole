add_executable(channelsgroup_test WIN32
    channelsgroup_test.cpp channelsgroup_test.h
)

target_include_directories(channelsgroup_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(channelsgroup_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
