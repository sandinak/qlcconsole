include(../../src/include_ui.cmake)

set(module_name "surfacesetupui_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../../src
    ../../../engine/test/iopluginstub
    ../../../webaccess/src
)

include_ui_header(${module_name})

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
    iopluginstub   # its typeinfo, for UBSan vptr on static_cast
    qlcplusui
    qlcpluswebaccess
)

# The simulated PMJ Black 1 and its OpenDeck config (docs/dev/SURFACE_TESTS.md).
qlc_surfacesim(${module_name})
