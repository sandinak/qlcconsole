add_executable(qlcchannel_test WIN32
    qlcchannel_test.cpp qlcchannel_test.h
)
target_include_directories(qlcchannel_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcchannel_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
