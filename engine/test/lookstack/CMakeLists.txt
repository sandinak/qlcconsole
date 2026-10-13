add_executable(lookstack_test WIN32
    lookstack_test.cpp lookstack_test.h
)
target_include_directories(lookstack_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(lookstack_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
