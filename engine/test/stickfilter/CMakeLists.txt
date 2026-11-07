add_executable(stickfilter_test WIN32
    stickfilter_test.cpp stickfilter_test.h
    # The HID plugin's stick rest correction: plain Qt Core, tested here
    # without the plugin, a controller or a clock.
    ../../../plugins/hid/stickfilter.cpp ../../../plugins/hid/stickfilter.h
)
target_include_directories(stickfilter_test PRIVATE
    ../../../plugins/hid
)

target_link_libraries(stickfilter_test PRIVATE
    Qt6::Core
    Qt6::Test
)
