add_executable(powerdistribution_test WIN32
    powerdistribution_test.cpp powerdistribution_test.h
)
target_include_directories(powerdistribution_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(powerdistribution_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
