include(../../src/include_ui.cmake)

set(module_name "mainbar_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

# A real show to measure and picture the window with (copied before use, so
# autosave never writes beside the original).
target_compile_definitions(${module_name} PRIVATE
    MAINBAR_DEMO_SHOW="${CMAKE_SOURCE_DIR}/test-workspaces/help-demo.qxw")

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../../src
    ../../src/showmanager
    ../../src/monitor
    ../../src/virtualconsole
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
