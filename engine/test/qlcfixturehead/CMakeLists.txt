add_executable(qlcfixturehead_test WIN32
    qlcfixturehead_test.cpp qlcfixturehead_test.h
)
target_include_directories(qlcfixturehead_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcfixturehead_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
