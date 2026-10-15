add_executable(spreadlayer_test WIN32
    spreadlayer_test.cpp spreadlayer_test.h
)
target_include_directories(spreadlayer_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(spreadlayer_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
