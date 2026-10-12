add_executable(spreadlayer_test WIN32
    spreadlayer_test.cpp spreadlayer_test.h
)
target_include_directories(spreadlayer_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(spreadlayer_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
