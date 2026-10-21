add_executable(controllerkeepalive_test WIN32
    controllerkeepalive_test.cpp controllerkeepalive_test.h
)
target_include_directories(controllerkeepalive_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(controllerkeepalive_test PRIVATE
    Qt6::Core
    Qt6::Test
    qlcplusengine
)
