add_executable(lookstack_test WIN32
    lookstack_test.cpp lookstack_test.h
)
target_include_directories(lookstack_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(lookstack_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
