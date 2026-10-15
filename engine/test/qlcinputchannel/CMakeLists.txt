add_executable(qlcinputchannel_test WIN32
    qlcinputchannel_test.cpp qlcinputchannel_test.h
)
target_include_directories(qlcinputchannel_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcinputchannel_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
