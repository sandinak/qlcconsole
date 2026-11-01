add_executable(mcpserver_test WIN32
    mcpserver_test.cpp mcpserver_test.h
)
target_link_libraries(mcpserver_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
    qlcconsoleai
)
