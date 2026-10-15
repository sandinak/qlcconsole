include(../../src/include_ui.cmake)

set(module_name "efxpreviewarea_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../src
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
    qlcplusui
)

include_ui_header(${module_name})

target_link_libraries(${module_name} PRIVATE
    Qt6::Qml
)

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
