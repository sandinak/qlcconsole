add_executable(timecodesource_test WIN32
    timecodesource_test.cpp timecodesource_test.h
)
target_include_directories(timecodesource_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(timecodesource_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
