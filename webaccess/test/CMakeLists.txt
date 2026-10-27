find_package(Qt6 REQUIRED COMPONENTS WebSockets)
include(../../ui/src/include_ui.cmake)

set(module_name "webaccess_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
    ../../ui/test/common/testsettings.cpp
)

target_include_directories(${module_name} PRIVATE
    ../src
    ../src/qhttpserver
    ../../engine/src
    ../../engine/test/common
    ../../plugins/interfaces
    ../../ui/src
    ../../ui/src/virtualconsole
)

include_ui_header(${module_name})

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    Qt6::WebSockets
    Qt6::Widgets
    qlcplusengine
    qlcplusui
    qlcpluswebaccess
)
