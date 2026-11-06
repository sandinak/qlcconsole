add_executable(showreviewshows_test WIN32
    showreviewshows_test.cpp showreviewshows_test.h
)
target_include_directories(showreviewshows_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

# The shows students open first: the course's and the bundled sample.
target_compile_definitions(showreviewshows_test PRIVATE
    COURSE_SHOWS_DIR="${CMAKE_SOURCE_DIR}/docs/course/shows"
    SAMPLE_SHOW="${CMAKE_SOURCE_DIR}/resources/samples/Sample.qxw"
)

target_link_libraries(showreviewshows_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
