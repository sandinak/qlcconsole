add_executable(aimsolver_test WIN32
    aimsolver_test.cpp aimsolver_test.h
)
target_include_directories(aimsolver_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(aimsolver_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
