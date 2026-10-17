add_executable(measurementunits_test WIN32
    measurementunits_test.cpp measurementunits_test.h
)
target_include_directories(measurementunits_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(measurementunits_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
