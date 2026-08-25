add_executable(qxwimporter_test WIN32
    qxwimporter_test.cpp qxwimporter_test.h
)
target_include_directories(qxwimporter_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qxwimporter_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
