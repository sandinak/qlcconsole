include(../../src/include_ui.cmake)

set(module_name "officeshow_test")

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
    qlcplusengine
    qlcplusui
)

# The two-head ring mover definition the office rig uses lives with the
# engine tests' fixtures.
target_compile_definitions(${module_name} PRIVATE
    TEST_FIXTURES_DIR="${CMAKE_SOURCE_DIR}/engine/test/common/fixtures"
)
