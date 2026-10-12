add_executable(speedmasters_test WIN32
    speedmasters_test.cpp speedmasters_test.h
)
target_include_directories(speedmasters_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(speedmasters_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
