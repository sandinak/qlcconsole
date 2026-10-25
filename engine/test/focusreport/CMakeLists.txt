add_executable(focusreport_test WIN32
    focusreport_test.cpp focusreport_test.h
)
target_include_directories(focusreport_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../iopluginstub
)

target_link_libraries(focusreport_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
