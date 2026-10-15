add_executable(showcontrol_test WIN32
    showcontrol_test.cpp showcontrol_test.h
)
target_include_directories(showcontrol_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(showcontrol_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
