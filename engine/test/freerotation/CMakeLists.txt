add_executable(freerotation_test WIN32
    freerotation_test.cpp freerotation_test.h
)
target_include_directories(freerotation_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(freerotation_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
