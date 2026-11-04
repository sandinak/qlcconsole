include(../../src/include_ui.cmake)

set(module_name "menuwords_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../src
)

include_ui_header(${module_name})

# It reads the UI's sources: every menu label in ui/src, as written.
target_compile_definitions(${module_name} PRIVATE
    UI_SRC_DIR="${CMAKE_SOURCE_DIR}/ui/src")

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
    qlcplusui
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Qml
)
