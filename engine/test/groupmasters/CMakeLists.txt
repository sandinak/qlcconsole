add_executable(groupmasters_test WIN32
    ../common/resource_paths.h
    groupmasters_test.cpp groupmasters_test.h
)
target_include_directories(groupmasters_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(groupmasters_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
