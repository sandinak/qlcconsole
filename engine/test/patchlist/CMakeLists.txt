add_executable(patchlist_test WIN32
    patchlist_test.cpp patchlist_test.h
)
target_include_directories(patchlist_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(patchlist_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
