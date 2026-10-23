add_executable(outputrules_test WIN32
    ../common/resource_paths.h
    outputrules_test.cpp outputrules_test.h
)
target_include_directories(outputrules_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
    ../iopluginstub
)

target_link_libraries(outputrules_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
