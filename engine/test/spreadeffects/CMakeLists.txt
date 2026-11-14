add_executable(spreadeffects_test WIN32
    spreadeffects_test.cpp spreadeffects_test.h
)
target_include_directories(spreadeffects_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(spreadeffects_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)

# The show's own pixel bars (DanceHysteria's Step Row 64 Heads and
# PL-4003 1 Bar definitions, copied: never the show itself).
target_compile_definitions(spreadeffects_test PRIVATE
    SPREADEFFECTS_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data/"
)
