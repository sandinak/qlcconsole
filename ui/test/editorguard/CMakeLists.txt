include(../../src/include_ui.cmake)

set(module_name "editorguard_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../engine/audio/src
    ../../../plugins/interfaces
    ../../src
    ../../src/showmanager
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

# Qml on every Qt: the look editor's effects need QJSValue.
target_link_libraries(${module_name} PRIVATE
    Qt6::Qml
)
