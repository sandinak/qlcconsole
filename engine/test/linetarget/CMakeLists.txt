add_executable(linetarget_test WIN32
    linetarget_test.cpp linetarget_test.h
)
target_include_directories(linetarget_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(linetarget_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
