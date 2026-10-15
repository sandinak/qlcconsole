add_executable(inventory_test WIN32
    inventory_test.cpp inventory_test.h
)
target_include_directories(inventory_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(inventory_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
