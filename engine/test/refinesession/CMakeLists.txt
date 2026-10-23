add_executable(refinesession_test WIN32
    refinesession_test.cpp refinesession_test.h
)
target_include_directories(refinesession_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(refinesession_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
