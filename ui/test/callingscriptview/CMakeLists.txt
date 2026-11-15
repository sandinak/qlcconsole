include(../../src/include_ui.cmake)

set(module_name "callingscriptview_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../src
)

include_ui_header(${module_name})

# The script excerpt the Cues from the script tests read (one copy, the engine test's).
target_compile_definitions(${module_name} PRIVATE
    SCRIPT_EXCERPT="${CMAKE_CURRENT_SOURCE_DIR}/../../../engine/test/callingscript/script-excerpt.txt"
)

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

