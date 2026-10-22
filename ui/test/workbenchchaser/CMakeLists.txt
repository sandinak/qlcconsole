include(../../src/include_ui.cmake)

set(module_name "workbenchchaser_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

# Real shows, read from copies (never the originals): DanceHysteria-0.1's
# "Full Show" (a chaser of collections) and the stock Sample.qxw.
target_compile_definitions(${module_name} PRIVATE
    WB_DH_SHOW="${CMAKE_SOURCE_DIR}/test-workspaces/DanceHysteria-0.1.qlcc"
    WB_SAMPLE_SHOW="${CMAKE_SOURCE_DIR}/resources/samples/Sample.qxw")

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
    qlcplusengine
    qlcplusui
)

# Qml on every Qt: the test drives a running effect, and effectinstance.h
# needs QJSValue (QtQml on Qt5 too).
target_link_libraries(${module_name} PRIVATE
    Qt6::Qml
)

