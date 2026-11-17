include(../../src/include_ui.cmake)

set(module_name "firespreadperf_test")

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

# The stage show rigroomlight_test draws (a copy of a real rig, kept with it).
target_compile_definitions(${module_name} PRIVATE
    STAGE_SHOW="${CMAKE_CURRENT_SOURCE_DIR}/../rigroomlight/stage-show.qlcc"
)
