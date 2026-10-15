add_executable(keypadparser_test WIN32
    keypadparser_test.cpp keypadparser_test.h
)
target_include_directories(keypadparser_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(keypadparser_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
