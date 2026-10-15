add_executable(effectinstance_test WIN32
    effectinstance_test.cpp effectinstance_test.h
)
target_include_directories(effectinstance_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(effectinstance_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
