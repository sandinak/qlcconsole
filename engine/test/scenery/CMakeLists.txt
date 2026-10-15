add_executable(scenery_test WIN32
    scenery_test.cpp scenery_test.h
)
target_include_directories(scenery_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(scenery_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
