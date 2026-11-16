# The scan and open threads: a controller that stops answering while it is
# opened never holds the app (a fake HIDBackend, no real device).
add_executable(hidrescan_test WIN32 MACOSX_BUNDLE
    hidrescan_test.cpp hidrescan_test.h fakehidbackend.h
)
target_include_directories(hidrescan_test PRIVATE
    ..
    ../../interfaces
    ../../../engine/src
)
target_compile_definitions(hidrescan_test PRIVATE
    QLC_TEST_RESOURCE_ROOT="${CMAKE_SOURCE_DIR}/resources/")
target_link_libraries(hidrescan_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Test
    hidplugin
)

if(UNIX AND NOT APPLE)
    # Linux only: the joystick reader (/dev/input/jsN) when its pad goes and
    # comes back, a FIFO for the node.
    add_executable(hidlinuxjoystick_test
        hidlinuxjoystick_test.cpp hidlinuxjoystick_test.h
    )
    target_include_directories(hidlinuxjoystick_test PRIVATE
        ..
        ../linux
        ../../interfaces
        ../../../engine/src
    )
    target_link_libraries(hidlinuxjoystick_test PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Test
        hidplugin
    )
endif()

if(APPLE)
    # macOS only: the GameController framework side of the HID plugin.
    add_executable(hidgamecontroller_test WIN32 MACOSX_BUNDLE
        hidgamecontroller_test.mm hidgamecontroller_test.h
        ../macx/hidgamecontroller.mm ../macx/hidgamecontroller.h
        ../gamepadlayout.cpp ../gamepadlayout.h
    )
    set_source_files_properties(hidgamecontroller_test.mm ../macx/hidgamecontroller.mm
        PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
    target_include_directories(hidgamecontroller_test PRIVATE
        ..
        ../macx
        ../../interfaces
    )
    target_link_libraries(hidgamecontroller_test PRIVATE
        Qt6::Core
        Qt6::Test
        "-framework Foundation"
        "-framework GameController"
        "-framework CoreHaptics"
    )
endif()
