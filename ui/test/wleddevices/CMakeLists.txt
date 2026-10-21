include(../../src/include_ui.cmake)
add_compile_definitions(TEST_PLUGINS_DIR="${CMAKE_BINARY_DIR}/plugins")

set(module_name "wleddevices_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../src
)

include_ui_header(${module_name})

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    Qt6::Network
    qlcplusengine
    qlcplusui
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Qml
)

