set(module_name "fixtureeditorcheck_test")

# The fixture editor is an app, not a library: its repair dialog and
# problems panel are compiled in here on their own.
add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
    ../../../fixtureeditor/repairdialog.cpp ../../../fixtureeditor/repairdialog.h
    ../../../fixtureeditor/problemspanel.cpp ../../../fixtureeditor/problemspanel.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../../../fixtureeditor
)

target_compile_definitions(${module_name} PRIVATE
    FIXTUREDEFCHECK_DATA="${CMAKE_CURRENT_SOURCE_DIR}/../../../engine/test/fixturedefcheck/data/"
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
)
