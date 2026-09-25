add_executable(inventory_test WIN32
    inventory_test.cpp inventory_test.h
)
target_include_directories(inventory_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(inventory_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
