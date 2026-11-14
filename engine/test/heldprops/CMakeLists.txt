add_executable(heldprops_test WIN32
    heldprops_test.cpp heldprops_test.h
)
target_include_directories(heldprops_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(heldprops_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Qml
    Qt6::Test
    qlcplusengine
)
