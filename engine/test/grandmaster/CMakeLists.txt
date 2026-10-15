add_executable(grandmaster_test WIN32
    grandmaster_test.cpp grandmaster_test.h
)
target_include_directories(grandmaster_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(grandmaster_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
