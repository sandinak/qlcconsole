add_executable(hotpath_test WIN32
    ../common/resource_paths.h
    hotpath_test.cpp hotpath_test.h
)
target_include_directories(hotpath_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(hotpath_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
target_link_libraries(hotpath_test PRIVATE
    Qt6::Qml
)
