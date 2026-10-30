add_executable(legacysettings_test WIN32
    legacysettings_test.cpp legacysettings_test.h
)
target_include_directories(legacysettings_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(legacysettings_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
