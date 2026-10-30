add_executable(padnames_test WIN32
    padnames_test.cpp padnames_test.h
)
target_include_directories(padnames_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(padnames_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
