add_executable(cuestack_test WIN32
    ../common/resource_paths.h
    cuestack_test.cpp cuestack_test.h
)
target_include_directories(cuestack_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(cuestack_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
