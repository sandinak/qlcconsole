add_executable(channelmodifier_test WIN32
    channelmodifier_test.cpp channelmodifier_test.h
)
target_include_directories(channelmodifier_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(channelmodifier_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
