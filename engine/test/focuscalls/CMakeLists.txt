add_executable(focuscalls_test WIN32
    focuscalls_test.cpp focuscalls_test.h
)
target_include_directories(focuscalls_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../iopluginstub
)

target_link_libraries(focuscalls_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
