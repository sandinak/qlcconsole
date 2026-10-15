add_executable(qlcfixturemode_test WIN32
    qlcfixturemode_test.cpp qlcfixturemode_test.h
)
target_include_directories(qlcfixturemode_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcfixturemode_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
