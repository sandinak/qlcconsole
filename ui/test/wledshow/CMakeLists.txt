include(../../src/include_ui.cmake)
add_compile_definitions(TEST_PLUGINS_DIR="${CMAKE_BINARY_DIR}/plugins")
add_compile_definitions(TEST_STUB_DIR="${CMAKE_BINARY_DIR}/engine/test/iopluginstub")

set(module_name "wledshow_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../engine/test/iopluginstub
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
    Qt6::Network
    qlcplusengine
    iopluginstub   # its typeinfo, for the static_cast
    qlcplusui
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Qml
)

# The real WLED output plugin is on the path: built first.
if(TARGET wled)
    add_dependencies(${module_name} wled)
endif()
