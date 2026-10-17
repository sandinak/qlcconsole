add_executable(showtree_test WIN32
    showtree_test.cpp showtree_test.h
)
target_include_directories(showtree_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(showtree_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
