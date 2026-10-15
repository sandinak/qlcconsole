add_executable(patchlabels_test WIN32
    patchlabels_test.cpp patchlabels_test.h
)
target_include_directories(patchlabels_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(patchlabels_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
