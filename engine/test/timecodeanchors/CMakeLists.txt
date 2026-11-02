add_executable(timecodeanchors_test WIN32
    timecodeanchors_test.cpp timecodeanchors_test.h
)
target_include_directories(timecodeanchors_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(timecodeanchors_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
