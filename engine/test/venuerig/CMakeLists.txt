add_executable(venuerig_test WIN32
    venuerig_test.cpp venuerig_test.h
)
target_include_directories(venuerig_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../iopluginstub
)

target_link_libraries(venuerig_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
