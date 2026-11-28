add_executable(lightmap_test WIN32
    lightmap_test.cpp lightmap_test.h
)
target_include_directories(lightmap_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(lightmap_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
