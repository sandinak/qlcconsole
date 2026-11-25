add_executable(padsim_test WIN32
    padsim_test.cpp padsim_test.h
)
target_include_directories(padsim_test PRIVATE
    ../../src
)
target_link_libraries(padsim_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
# The simulated pads (docs/dev/SURFACE_TESTS.md), through the real HID
# plugin loaded as the app loads it, with a fake backend behind it.
qlc_surfacesim(padsim_test)
add_dependencies(padsim_test hidplugin)
target_compile_definitions(padsim_test PRIVATE
    QLC_HID_PLUGIN_FILE="$<TARGET_FILE:hidplugin>")
