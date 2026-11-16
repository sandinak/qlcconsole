add_executable(hidstuckpad_test WIN32
    hidstuckpad_test.cpp hidstuckpad_test.h
)
target_include_directories(hidstuckpad_test PRIVATE
    ../../../plugins/interfaces
    ../../../plugins/hid
    ../../../plugins/hid/test
    ../../src
)

target_link_libraries(hidstuckpad_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

# The real HID plugin, loaded as the app loads it, with a fake backend
# (plugins/hid/test/fakehidbackend.h) behind it.
add_dependencies(hidstuckpad_test hidplugin)
target_compile_definitions(hidstuckpad_test PRIVATE
    QLC_HID_PLUGIN_FILE="$<TARGET_FILE:hidplugin>")
