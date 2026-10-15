add_executable(qlccapability_test WIN32
    qlccapability_test.cpp qlccapability_test.h
)
target_include_directories(qlccapability_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlccapability_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
