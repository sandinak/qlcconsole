add_executable(script_test WIN32
    ../mastertimer/mastertimer_stub.cpp ../mastertimer/mastertimer_stub.h
    script_test.cpp script_test.h
)
target_include_directories(script_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

target_link_libraries(script_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
