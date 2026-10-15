add_executable(doc_test WIN32
    ../common/resource_paths.h
    doc_test.cpp doc_test.h
)
target_include_directories(doc_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(doc_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
