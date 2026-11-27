add_executable(audiomessage_test WIN32
    audiomessage_test.cpp audiomessage_test.h
)
target_include_directories(audiomessage_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(audiomessage_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
