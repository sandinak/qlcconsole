add_executable(sequence_test WIN32
    ../mastertimer/mastertimer_stub.cpp ../mastertimer/mastertimer_stub.h
    sequence_test.cpp sequence_test.h
)
target_include_directories(sequence_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

target_link_libraries(sequence_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
