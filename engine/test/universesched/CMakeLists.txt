add_executable(universesched_test WIN32
    ../common/resource_paths.h
    universesched_test.cpp universesched_test.h
)
target_include_directories(universesched_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(universesched_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Qml
    qlcplusengine
)
