include(../../src/include_ui.cmake)

set(module_name "showfixtureproperties_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

# Fixture Properties for every fixture of the show files every build has
# (test-workspaces/, the bundled sample) and of the course shows.
target_compile_definitions(${module_name} PRIVATE
    SHOWFILES_WORKSPACES="${CMAKE_SOURCE_DIR}/test-workspaces/"
    SHOWFILES_SAMPLE="${CMAKE_SOURCE_DIR}/resources/samples/Sample.qxw"
    SHOWFILES_COURSE="${CMAKE_SOURCE_DIR}/docs/course/shows/")

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
    qlcplusui
)
