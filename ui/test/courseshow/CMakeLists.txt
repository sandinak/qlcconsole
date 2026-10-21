include(../../src/include_ui.cmake)

set(module_name "courseshow_test")

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

# The course lives in the repo: its show files are read (and, with
# COURSE_RIG set, written) there.
target_compile_definitions(${module_name} PRIVATE
    COURSE_DIR="${CMAKE_SOURCE_DIR}/docs/course"
)
