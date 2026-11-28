include(../../src/include_ui.cmake)

set(module_name "plotsheet_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

# A real show's plot: stage-structures-demo.qxw from test-workspaces/.
target_compile_definitions(${module_name} PRIVATE
    PLOTSHEET_WORKSPACES="${CMAKE_SOURCE_DIR}/test-workspaces/")

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../../src
    ../../src/virtualconsole
    ../../src/showmanager
    ../../src/monitor
    ../../../engine/test/iopluginstub
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
)

# Where Qt has its PDF module, the test reads the text back out of the PDF
# itself; everywhere, it reads what the sheet painted.
find_package(Qt6 QUIET COMPONENTS Pdf)
if(TARGET Qt6::Pdf)
    target_link_libraries(${module_name} PRIVATE Qt6::Pdf)
    target_compile_definitions(${module_name} PRIVATE PLOTSHEET_HAVE_QTPDF=1)
endif()
