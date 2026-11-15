add_executable(goguard_test WIN32
    goguard_test.cpp goguard_test.h
)
target_include_directories(goguard_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
    ../iopluginstub
)

target_link_libraries(goguard_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
    iopluginstub   # its typeinfo, for UBSan vptr on static_cast
)
