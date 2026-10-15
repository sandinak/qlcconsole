add_executable(stagedoutput_test WIN32
    stagedoutput_test.cpp stagedoutput_test.h
)
target_include_directories(stagedoutput_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(stagedoutput_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
