include(../../src/include_ui.cmake)

set(module_name "rigroomlight_test")

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

# A real show for the review shots, when one is put beside the test
# (never committed): repro/show.qlcc.
target_compile_definitions(${module_name} PRIVATE
    RIGROOM_SRC_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
