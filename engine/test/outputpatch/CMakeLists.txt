add_executable(outputpatch_test WIN32
    outputpatch_test.cpp outputpatch_test.h
)
target_include_directories(outputpatch_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../iopluginstub
)

target_link_libraries(outputpatch_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

# Consider using qt_generate_deploy_app_script() for app deployment if
# the project can use Qt 6.3. In that case rerun qmake2cmake with
# --min-qt-version=6.3.
