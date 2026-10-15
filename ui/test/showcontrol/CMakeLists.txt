include(../../src/include_ui.cmake)

set(module_name "showcontrol_ui_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../src
)

include_ui_header(${module_name})

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    Qt${QT_MAJOR_VERSION}::Widgets
    qlcplusengine
    qlcplusui
)

target_link_libraries(${module_name} PRIVATE
    Qt${QT_MAJOR_VERSION}::Qml
)

