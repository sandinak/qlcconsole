add_executable(showfunction_test WIN32
    showfunction_test.cpp showfunction_test.h
)
target_include_directories(showfunction_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(showfunction_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
