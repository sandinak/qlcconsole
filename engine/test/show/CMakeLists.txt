add_executable(show_test WIN32
    show_test.cpp show_test.h
)
target_include_directories(show_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(show_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
