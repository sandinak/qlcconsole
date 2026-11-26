add_executable(editlive_test WIN32
    editlive_test.cpp editlive_test.h
)
target_include_directories(editlive_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)
target_link_libraries(editlive_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
# The simulated boards (docs/dev/SURFACE_TESTS.md).
qlc_surfacesim(editlive_test)
