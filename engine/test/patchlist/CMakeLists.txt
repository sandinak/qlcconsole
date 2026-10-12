add_executable(patchlist_test WIN32
    patchlist_test.cpp patchlist_test.h
)
target_include_directories(patchlist_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(patchlist_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
