add_executable(pixelboxes_test WIN32
    pixelboxes_test.cpp pixelboxes_test.h
)
target_include_directories(pixelboxes_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(pixelboxes_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)

# The course's classroom show is the rig: its pixel boxes, beams and groups.
target_compile_definitions(pixelboxes_test PRIVATE
    COURSE_DIR="${CMAKE_SOURCE_DIR}/docs/course"
)
