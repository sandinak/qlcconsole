add_executable(scenery_test WIN32
    scenery_test.cpp scenery_test.h
)
target_include_directories(scenery_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(scenery_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
