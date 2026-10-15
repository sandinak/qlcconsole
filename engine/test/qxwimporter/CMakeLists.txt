add_executable(qxwimporter_test WIN32
    qxwimporter_test.cpp qxwimporter_test.h
)
target_include_directories(qxwimporter_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(qxwimporter_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
