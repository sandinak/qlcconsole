set(module_name "editphysical_test")

# The fixture editor is an app, not a library: its Physical page and the
# unit-aware field it uses are compiled in here on their own.
add_executable(${module_name} WIN32 MACOSX_BUNDLE
    ${module_name}.cpp ${module_name}.h
    ../../../fixtureeditor/editphysical.cpp ../../../fixtureeditor/editphysical.h
    ../../../fixtureeditor/editphysical.ui
    ../../src/measurementedit.cpp ../../src/measurementedit.h
)

target_include_directories(${module_name} PRIVATE
    ../../../engine/src
    ../../../plugins/interfaces
    ../../../fixtureeditor
    ../../src
)

target_link_libraries(${module_name} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
)
