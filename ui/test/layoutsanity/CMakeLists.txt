include(../../src/include_ui.cmake)

set(module_name "layoutsanity_test")

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

# A finished course show is the show the screens open on (read-only), and
# the test fixtures give the colour look its wheel + ring fixture.
target_compile_definitions(${module_name} PRIVATE
    COURSE_DIR="${CMAKE_SOURCE_DIR}/docs/course"
    TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../engine/test/common/fixtures/"
)
