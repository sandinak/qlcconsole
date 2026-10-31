add_executable(padrun_test WIN32
    padrun_test.cpp padrun_test.h
)
target_include_directories(padrun_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../iopluginstub
)

target_link_libraries(padrun_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
