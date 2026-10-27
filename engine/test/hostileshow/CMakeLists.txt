add_executable(hostileshow_test WIN32
    ../common/resource_paths.h
    hostileshow_test.cpp hostileshow_test.h
)
target_include_directories(hostileshow_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(hostileshow_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
