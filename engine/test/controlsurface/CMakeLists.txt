add_executable(controlsurface_test WIN32
    controlsurface_test.cpp controlsurface_test.h
)
target_include_directories(controlsurface_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(controlsurface_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
