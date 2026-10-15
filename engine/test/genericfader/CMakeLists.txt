add_executable(genericfader_test WIN32
    ../common/resource_paths.h
    genericfader_test.cpp genericfader_test.h
)
target_include_directories(genericfader_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(genericfader_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
