add_executable(dmxusb_test WIN32 MACOSX_BUNDLE
    ../src/writehealth.cpp ../src/writehealth.h
    dmxusb_test.cpp dmxusb_test.h
)
target_include_directories(dmxusb_test PRIVATE
    ../src
    ../../../engine/test/common
)

target_link_libraries(dmxusb_test PRIVATE
    Qt6::Core
    Qt6::Test
)
