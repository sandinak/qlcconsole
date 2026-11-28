include(../../src/include_ui.cmake)

set(module_name "showscale_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
    # The made-up rig the size of the shows we run (board Q360)
    ../../../engine/test/common/scalerig.cpp ../../../engine/test/common/scalerig.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../engine/test/common
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
    Qt6::Qml
    qlcplusengine
    qlcplusui
)
if(WIN32)
    target_link_libraries(${module_name} PRIVATE psapi)
endif()
