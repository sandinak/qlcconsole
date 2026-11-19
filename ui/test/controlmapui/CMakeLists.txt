include(../../src/include_ui.cmake)

set(module_name "controlmapui_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../../src
    ../../../engine/test/iopluginstub
)

include_ui_header(${module_name})

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
    iopluginstub   # a patched input line, for a profile's fader
    qlcplusui
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Qml
)

