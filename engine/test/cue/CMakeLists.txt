add_executable(cue_test WIN32
    cue_test.cpp cue_test.h
)
target_include_directories(cue_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(cue_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
