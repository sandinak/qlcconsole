add_executable(scenevalue_test WIN32
    scenevalue_test.cpp scenevalue_test.h
)
target_include_directories(scenevalue_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(scenevalue_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
