add_executable(cuesheet_test WIN32
    cuesheet_test.cpp cuesheet_test.h
)
target_include_directories(cuesheet_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(cuesheet_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
