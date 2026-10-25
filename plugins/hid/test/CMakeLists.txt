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
