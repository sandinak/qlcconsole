add_executable(qlcclipboard_test WIN32
    qlcclipboard_test.cpp qlcclipboard_test.h
)
target_include_directories(qlcclipboard_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcclipboard_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
