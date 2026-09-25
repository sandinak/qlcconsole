add_executable(stagedoutput_test WIN32
    stagedoutput_test.cpp stagedoutput_test.h
)
target_include_directories(stagedoutput_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(stagedoutput_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
