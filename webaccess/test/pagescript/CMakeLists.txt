include(../../../ui/src/include_ui.cmake)

set(module_name "pagescript_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
    ../../../ui/test/common/testsettings.cpp
)

# The browser stand-ins the pages' scripts run on.
qt_add_resources(${module_name} "pagestubs"
    PREFIX "/"
    FILES pagestubs.js
)

target_include_directories(${module_name} PRIVATE
    ../../src
    ../../../engine/src
    ../../../engine/test/common
    ../../../ui/src
)

include_ui_header(${module_name})

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
    qlcplusui
    qlcpluswebaccess
)
