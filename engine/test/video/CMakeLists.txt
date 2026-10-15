add_executable(video_test WIN32
    ../mastertimer/mastertimer_stub.cpp ../mastertimer/mastertimer_stub.h
    video_test.cpp video_test.h
)
target_include_directories(video_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

target_link_libraries(video_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
