add_executable(ltc_test WIN32
    ltc_test.cpp ltc_test.h
)
target_include_directories(ltc_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(ltc_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
