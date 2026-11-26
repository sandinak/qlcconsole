add_executable(hazardarming_test WIN32
    hazardarming_test.cpp hazardarming_test.h
)
target_include_directories(hazardarming_test PRIVATE
    ../../src
    ../common
)
target_link_libraries(hazardarming_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
# The spotter's board, as the board sends it (docs/dev/SURFACE_TESTS.md).
qlc_surfacesim(hazardarming_test)
