add_executable(qlcfile_test WIN32
    qlcfile_test.cpp qlcfile_test.h
)
target_include_directories(qlcfile_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcfile_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
