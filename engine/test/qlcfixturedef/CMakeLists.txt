add_executable(qlcfixturedef_test WIN32
    qlcfixturedef_test.cpp qlcfixturedef_test.h
)
target_include_directories(qlcfixturedef_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcfixturedef_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
