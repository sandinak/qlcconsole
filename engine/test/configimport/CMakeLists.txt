add_executable(configimport_test WIN32
    configimport_test.cpp configimport_test.h
)
target_include_directories(configimport_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(configimport_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
target_include_directories(configimport_test PRIVATE ../iopluginstub)
target_compile_definitions(configimport_test PRIVATE
    CONFIGIMPORT_SAMPLES="${CMAKE_CURRENT_SOURCE_DIR}")
