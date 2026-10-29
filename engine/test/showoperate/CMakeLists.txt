add_executable(showoperate_test WIN32
    showoperate_test.cpp showoperate_test.h
)
target_include_directories(showoperate_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(showoperate_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
