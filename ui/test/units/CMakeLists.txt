include(../../src/include_ui.cmake)

set(module_name "units_test")

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

# It reads the sources: every user string in ui/src, webaccess/src and
# engine/src, as written.
target_compile_definitions(${module_name} PRIVATE
    SOURCE_ROOT_DIR="${CMAKE_SOURCE_DIR}")

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
