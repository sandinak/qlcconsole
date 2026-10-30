add_executable(showtreesections_test WIN32
    showtreesections_test.cpp showtreesections_test.h
)
target_include_directories(showtreesections_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(showtreesections_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
