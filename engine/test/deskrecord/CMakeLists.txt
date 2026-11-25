add_executable(deskrecord_test WIN32
    deskrecord_test.cpp deskrecord_test.h
)
target_include_directories(deskrecord_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
    ../iopluginstub
)

target_link_libraries(deskrecord_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
    iopluginstub   # its typeinfo, for UBSan vptr on static_cast
)
