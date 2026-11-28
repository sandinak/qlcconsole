include(../../src/include_ui.cmake)

set(module_name "lightmapview_test")

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

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    Qt6::Network
    qlcplusengine
    qlcplusui
)

# The school stage rig rigroomlight_test reads, for the shots and the cost.
target_compile_definitions(${module_name} PRIVATE
    STAGE_SHOW="${CMAKE_CURRENT_SOURCE_DIR}/../rigroomlight/stage-show.qlcc"
)
