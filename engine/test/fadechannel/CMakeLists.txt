add_executable(fadechannel_test WIN32
    ../common/resource_paths.h
    fadechannel_test.cpp fadechannel_test.h
)
target_include_directories(fadechannel_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(fadechannel_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
