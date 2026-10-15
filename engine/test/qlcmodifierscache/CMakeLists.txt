add_executable(qlcmodifierscache_test WIN32
    qlcmodifierscache_test.cpp qlcmodifierscache_test.h
)
target_include_directories(qlcmodifierscache_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcmodifierscache_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
