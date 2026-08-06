add_executable(cueoutput_test WIN32
    cueoutput_test.cpp cueoutput_test.h
)
target_include_directories(cueoutput_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(cueoutput_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
