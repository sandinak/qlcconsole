add_executable(cueoutput_test WIN32
    cueoutput_test.cpp cueoutput_test.h
)
target_include_directories(cueoutput_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(cueoutput_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
