add_executable(showrunner_test WIN32
    showrunner_test.cpp showrunner_test.h
)
target_include_directories(showrunner_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(showrunner_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
