add_executable(aiassist_test WIN32
    aiassist_test.cpp aiassist_test.h
)
target_link_libraries(aiassist_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Test
    Qt6::Widgets
    qlcplusengine
    qlcconsoleai
)
