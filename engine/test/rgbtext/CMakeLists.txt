add_executable(rgbtext_test WIN32
    rgbtext_test.cpp rgbtext_test.h
)
target_include_directories(rgbtext_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../mastertimer
)

target_link_libraries(rgbtext_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
