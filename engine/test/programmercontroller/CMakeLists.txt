add_executable(programmercontroller_test WIN32
    ../common/resource_paths.h
    programmercontroller_test.cpp programmercontroller_test.h
)
target_include_directories(programmercontroller_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(programmercontroller_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
