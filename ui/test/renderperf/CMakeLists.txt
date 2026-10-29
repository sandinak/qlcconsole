include(../../src/include_ui.cmake)

set(module_name "renderperf_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
    # The mixed rig the stress harness and engine/test/hotpath run
    ../../../stresstest/engine/showbuilder.cpp ../../../stresstest/engine/showbuilder.h
)
target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../engine/test/common
    ../../../plugins/interfaces
    ../../../stresstest/engine
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
