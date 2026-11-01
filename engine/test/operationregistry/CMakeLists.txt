add_executable(operationregistry_test WIN32
    operationregistry_test.cpp operationregistry_test.h
)
target_include_directories(operationregistry_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(operationregistry_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
