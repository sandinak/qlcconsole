add_executable(channelsgroup_test WIN32
    channelsgroup_test.cpp channelsgroup_test.h
)

target_include_directories(channelsgroup_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(channelsgroup_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
