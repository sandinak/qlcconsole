add_executable(networktopology_test WIN32
    networktopology_test.cpp networktopology_test.h
)
target_include_directories(networktopology_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(networktopology_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
