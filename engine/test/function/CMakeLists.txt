add_executable(function_test WIN32
    function_stub.cpp function_stub.h
    function_test.cpp function_test.h
)
target_include_directories(function_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(function_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
