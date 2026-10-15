add_executable(rgbscript_test WIN32
    ../common/resource_paths.h
    rgbscript_test.cpp rgbscript_test.h
)
target_include_directories(rgbscript_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

target_link_libraries(rgbscript_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)

target_link_libraries(rgbscript_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Qml
)

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
