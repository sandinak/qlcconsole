add_executable(palettenaming_test WIN32
    palettenaming_test.cpp palettenaming_test.h
)
target_include_directories(palettenaming_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(palettenaming_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
