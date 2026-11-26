add_executable(surfacepages_test WIN32
    surfacepages_test.cpp surfacepages_test.h
)
target_include_directories(surfacepages_test PRIVATE
    ../../src
)
target_link_libraries(surfacepages_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
# The simulated boards (docs/dev/SURFACE_TESTS.md).
qlc_surfacesim(surfacepages_test)
