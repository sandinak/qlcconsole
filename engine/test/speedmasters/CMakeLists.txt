add_executable(speedmasters_test WIN32
    speedmasters_test.cpp speedmasters_test.h
)
target_include_directories(speedmasters_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(speedmasters_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
