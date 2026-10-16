add_executable(showembedding_test WIN32
    ../common/resource_paths.h
    showembedding_test.cpp showembedding_test.h
)
target_include_directories(showembedding_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(showembedding_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
