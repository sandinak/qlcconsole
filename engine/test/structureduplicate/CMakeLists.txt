add_executable(structureduplicate_test WIN32
    structureduplicate_test.cpp structureduplicate_test.h
)
target_include_directories(structureduplicate_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(structureduplicate_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
