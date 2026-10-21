add_executable(gamepadlayout_test WIN32
    gamepadlayout_test.cpp gamepadlayout_test.h
    # The HID plugin's layout tables and profile reader: plain Qt Core, so
    # they are tested here without the plugin or a controller.
    ../../../plugins/hid/gamepadlayout.cpp ../../../plugins/hid/gamepadlayout.h
    ../../../plugins/hid/hidprofile.cpp ../../../plugins/hid/hidprofile.h
)
target_include_directories(gamepadlayout_test PRIVATE
    ../../../plugins/interfaces
    ../../../plugins/hid
    ../../src
)

target_link_libraries(gamepadlayout_test PRIVATE
    Qt6::Core
    Qt6::Test
    qlcplusengine
)
