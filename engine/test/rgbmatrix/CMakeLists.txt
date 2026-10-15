add_executable(rgbmatrix_test WIN32
    ../common/resource_paths.h
    ../mastertimer/mastertimer_stub.cpp ../mastertimer/mastertimer_stub.h
    rgbmatrix_test.cpp rgbmatrix_test.h
)
target_include_directories(rgbmatrix_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

target_link_libraries(rgbmatrix_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

target_link_libraries(rgbmatrix_test PRIVATE
    Qt6::Qml
)

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
