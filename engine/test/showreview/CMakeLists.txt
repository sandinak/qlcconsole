add_executable(showreview_test WIN32
    showreview_test.cpp showreview_test.h
)
target_include_directories(showreview_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(showreview_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
