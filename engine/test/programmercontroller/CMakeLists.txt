add_executable(programmercontroller_test WIN32
    ../common/resource_paths.h
    programmercontroller_test.cpp programmercontroller_test.h
)
target_include_directories(programmercontroller_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(programmercontroller_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
