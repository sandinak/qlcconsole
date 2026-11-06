include(../../src/include_ui.cmake)

set(module_name "librarybuild_test")

add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../src
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

# Qml on every Qt: the test drives a running effect, and effectinstance.h
# needs QJSValue (QtQml on Qt5 too).
target_link_libraries(${module_name} PRIVATE
    Qt6::Qml
)


# The command line's AI path against a model on loopback (ai/test/common):
# AiAssist is the one linked into qlcplusui.
if(QLC_AI_ASSIST)
    target_include_directories(${module_name} PRIVATE ../../../ai/src ../../../ai/test/common)
    target_compile_definitions(${module_name} PRIVATE QLC_AI_ASSIST)
    target_link_libraries(${module_name} PRIVATE Qt6::Network)
endif()
