add_executable(genericdmxsource_test WIN32
    genericdmxsource_test.cpp genericdmxsource_test.h
)
target_include_directories(genericdmxsource_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(genericdmxsource_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
