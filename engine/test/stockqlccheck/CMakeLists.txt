add_executable(stockqlccheck_test WIN32
    stockqlccheck_test.cpp stockqlccheck_test.h
)
target_include_directories(stockqlccheck_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(stockqlccheck_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
