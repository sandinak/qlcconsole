add_executable(track_test WIN32
    track_test.cpp track_test.h
)
target_include_directories(track_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(track_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
