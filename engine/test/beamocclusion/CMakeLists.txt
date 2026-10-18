add_executable(beamocclusion_test WIN32
    beamocclusion_test.cpp beamocclusion_test.h
)
target_include_directories(beamocclusion_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(beamocclusion_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
