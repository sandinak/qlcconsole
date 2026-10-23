include(../../src/include_ui.cmake)

set(module_name "eosimportwizard_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_compile_definitions(${module_name} PRIVATE
    EOS_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/test-workspaces/eos-samples/"
    EOS_FIXTURE_DIR="${CMAKE_SOURCE_DIR}/resources/fixtures/")

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../../src
    ../../src/virtualconsole
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
