add_executable(qlcinputprofile_test WIN32
    ../common/resource_paths.h
    qlcinputprofile_test.cpp qlcinputprofile_test.h
)
target_include_directories(qlcinputprofile_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcinputprofile_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
