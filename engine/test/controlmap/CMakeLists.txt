add_executable(controlmap_test WIN32
    controlmap_test.cpp controlmap_test.h
)
target_include_directories(controlmap_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
    ../iopluginstub
)

target_link_libraries(controlmap_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
