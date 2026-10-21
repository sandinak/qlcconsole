add_executable(trackback_test WIN32
    trackback_test.cpp trackback_test.h
)
target_include_directories(trackback_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(trackback_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

# The course shows the test plays (docs/course/shows).
target_compile_definitions(trackback_test PRIVATE
    TRACKBACK_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)
