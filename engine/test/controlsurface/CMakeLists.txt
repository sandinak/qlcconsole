add_executable(controlsurface_test WIN32
    controlsurface_test.cpp controlsurface_test.h
)
target_include_directories(controlsurface_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(controlsurface_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
