add_executable(showtreeplay_test WIN32
    showtreeplay_test.cpp showtreeplay_test.h
)
target_include_directories(showtreeplay_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(showtreeplay_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
