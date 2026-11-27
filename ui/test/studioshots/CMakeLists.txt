include(../../src/include_ui.cmake)

set(module_name "studioshots_test")

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
    Qt6::Network
    qlcplusengine
    qlcplusui
)

# Built on this branch and, with only the old API, at the commit before it
# (the before / after pictures of the Lighting Studio small wins).
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../studiowins)
    target_compile_definitions(${module_name} PRIVATE STUDIO_NEW=1)
endif()
