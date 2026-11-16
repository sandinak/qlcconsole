add_executable(iolock_test WIN32
    iolock_test.cpp iolock_test.h
)
target_include_directories(iolock_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(iolock_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
