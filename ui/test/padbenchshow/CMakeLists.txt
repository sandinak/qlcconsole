include(../../src/include_ui.cmake)

set(module_name "padbenchshow_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../../src
    ../../src/monitor
)

include_ui_header(${module_name})

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
    qlcplusui
)

# The real HID plugin, loaded as the app loads it (with virtual pads), and
# the committed show, checked as it is.
add_dependencies(${module_name} hidplugin)
target_compile_definitions(${module_name} PRIVATE
    QLC_HID_PLUGIN_FILE="$<TARGET_FILE:hidplugin>"
    PADBENCH_COMMITTED="${CMAKE_SOURCE_DIR}/test-workspaces/PadBench.qlcc")
