add_executable(followspotfeel_test WIN32
    followspotfeel_test.cpp followspotfeel_test.h
)
target_include_directories(followspotfeel_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(followspotfeel_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
