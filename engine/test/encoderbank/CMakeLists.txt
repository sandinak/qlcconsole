add_executable(encoderbank_test WIN32
    encoderbank_test.cpp encoderbank_test.h
)
target_include_directories(encoderbank_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
    ../iopluginstub
)

target_link_libraries(encoderbank_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
