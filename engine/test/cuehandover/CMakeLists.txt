add_executable(cuehandover_test WIN32
    cuehandover_test.cpp cuehandover_test.h
)
target_include_directories(cuehandover_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(cuehandover_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
