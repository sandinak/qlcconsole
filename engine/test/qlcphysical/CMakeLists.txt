add_executable(qlcphysical_test WIN32
    qlcphysical_test.cpp qlcphysical_test.h
)
target_include_directories(qlcphysical_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qlcphysical_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
