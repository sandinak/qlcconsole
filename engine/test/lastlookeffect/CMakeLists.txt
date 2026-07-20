add_executable(lastlookeffect_test WIN32
    lastlookeffect_test.cpp lastlookeffect_test.h
)
target_include_directories(lastlookeffect_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(lastlookeffect_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
