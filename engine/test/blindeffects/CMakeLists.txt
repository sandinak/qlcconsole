add_executable(blindeffects_test WIN32
    blindeffects_test.cpp blindeffects_test.h
)
target_include_directories(blindeffects_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(blindeffects_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
