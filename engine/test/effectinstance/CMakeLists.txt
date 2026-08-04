add_executable(effectinstance_test WIN32
    effectinstance_test.cpp effectinstance_test.h
)
target_include_directories(effectinstance_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(effectinstance_test PRIVATE
    Qt${QT_MAJOR_VERSION}::Core
    Qt${QT_MAJOR_VERSION}::Gui
    Qt${QT_MAJOR_VERSION}::Qml
    Qt${QT_MAJOR_VERSION}::Test
    qlcplusengine
)
