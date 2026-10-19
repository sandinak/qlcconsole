add_executable(focussession_test WIN32
    focussession_test.cpp focussession_test.h
    # The HID profile reader, to check the shipped Xbox profile maps every
    # control Focus mode uses (plain Qt Core, no plugin needed).
    ../../../plugins/hid/hidprofile.cpp ../../../plugins/hid/hidprofile.h
)
target_include_directories(focussession_test PRIVATE
    ../../../plugins/interfaces
    ../../../plugins/hid
    ../../src
    ../iopluginstub
)

target_link_libraries(focussession_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
