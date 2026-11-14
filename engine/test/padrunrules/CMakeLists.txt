add_executable(padrunrules_test WIN32
    padrunrules_test.cpp padrunrules_test.h
)
target_include_directories(padrunrules_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../iopluginstub
)

target_link_libraries(padrunrules_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
    iopluginstub   # its typeinfo, for UBSan vptr on static_cast
)
