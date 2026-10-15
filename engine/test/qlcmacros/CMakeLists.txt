add_executable(qlcmacros_test WIN32
    qlcmacros_test.cpp qlcmacros_test.h
)
target_include_directories(qlcmacros_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcmacros_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
