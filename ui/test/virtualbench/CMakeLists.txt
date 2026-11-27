include(../../src/include_ui.cmake)

set(module_name "virtualbench_test")

# The virtual bench (docs/dev/SURFACE_TESTS.md): his boards as CoreMIDI
# virtual ports, found by the real MIDI plugin. macOS only.
add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
    virtualmidi.cpp virtualmidi.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../../src
    ../../../webaccess/src
)

include_ui_header(${module_name})

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
    qlcplusui
    qlcpluswebaccess
    "-framework CoreMIDI"
    "-framework CoreFoundation"
)

add_dependencies(${module_name} midiplugin)
target_compile_definitions(${module_name} PRIVATE
    QLC_MIDI_PLUGIN_FILE="$<TARGET_FILE:midiplugin>")
