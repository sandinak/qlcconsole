add_executable(controllerjobs_test WIN32
    controllerjobs_test.cpp controllerjobs_test.h
)
target_include_directories(controllerjobs_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../iopluginstub
)

target_link_libraries(controllerjobs_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
    iopluginstub   # its typeinfo, for UBSan vptr on static_cast
)
