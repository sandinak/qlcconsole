# The UX walker (docs/dev/UX_WALKER.md): opt-in, never in the gate -- it is
# slow. cmake -DQLC_UXWALK=ON, then `make ux-walk` from the repo root.
include(../../ui/src/include_ui.cmake)

set(module_name "uxwalk")

add_executable(${module_name}
    uxwalk.cpp
    # The screen registry and the layout checker: layoutsanity_test's own
    # (its source is included by uxwalk.cpp; its header is moc'ed here).
    ../../ui/test/layoutsanity/layoutsanity_test.h
    # Settings of its own, never the person's.
    ../../ui/test/common/testsettings.cpp
)

target_include_directories(${module_name} PRIVATE
    ../../engine/src
    ../../plugins/interfaces
    ../../ui/src
    ../../ui/src/monitor
    ../../ui/test/layoutsanity
)

include_ui_header(${module_name})

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
    qlcplusui
)

target_compile_definitions(${module_name} PRIVATE
    QLC_TEST_RESOURCE_ROOT="${CMAKE_SOURCE_DIR}/resources/"
    COURSE_DIR="${CMAKE_SOURCE_DIR}/docs/course"
    TEST_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/engine/test/common/fixtures/"
    # The WLED output the flows add a WLED through (libwled, built beside it).
    UXWALK_PLUGINS_DIR="${CMAKE_BINARY_DIR}/plugins"
)

# F-11 drives a WLED through the real WLED output plugin.
if(TARGET wled)
    add_dependencies(${module_name} wled)
endif()
