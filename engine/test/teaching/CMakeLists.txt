add_executable(teaching_test WIN32
    teaching_test.cpp teaching_test.h
)
target_include_directories(teaching_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(teaching_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
