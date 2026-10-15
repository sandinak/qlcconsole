add_executable(rgbtext_test WIN32
    rgbtext_test.cpp rgbtext_test.h
)
target_include_directories(rgbtext_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

target_link_libraries(rgbtext_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
