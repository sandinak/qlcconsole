add_executable(scene_test WIN32
    ../common/resource_paths.h
    ../mastertimer/mastertimer_stub.cpp ../mastertimer/mastertimer_stub.h
    scene_test.cpp scene_test.h
)
target_include_directories(scene_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../iopluginstub
    ../mastertimer
)

target_link_libraries(scene_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
