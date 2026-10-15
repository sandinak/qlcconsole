add_executable(rgbalgorithm_test WIN32
    ../common/resource_paths.h
    rgbalgorithm_test.cpp rgbalgorithm_test.h
)
target_include_directories(rgbalgorithm_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

target_link_libraries(rgbalgorithm_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

target_link_libraries(rgbalgorithm_test PRIVATE
    Qt6::Qml
)
