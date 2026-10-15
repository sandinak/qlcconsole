add_executable(rgbplain_test WIN32
    rgbplain_test.cpp rgbplain_test.h
)
target_include_directories(rgbplain_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(rgbplain_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
