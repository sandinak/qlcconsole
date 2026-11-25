add_executable(surfacesim_test WIN32
    surfacesim_test.cpp surfacesim_test.h
    ../../../plugins/hid/hidprofile.cpp ../../../plugins/hid/hidprofile.h
)
target_include_directories(surfacesim_test PRIVATE
    ../../src
)
target_link_libraries(surfacesim_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
target_compile_definitions(surfacesim_test PRIVATE
    SURFACESIM_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
# The simulated boards and pads (docs/dev/SURFACE_TESTS.md).
qlc_surfacesim(surfacesim_test)
