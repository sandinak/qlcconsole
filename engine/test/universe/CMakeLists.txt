add_executable(universe_test WIN32
    universe_test.cpp universe_test.h
)
target_include_directories(universe_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(universe_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
