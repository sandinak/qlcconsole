add_executable(whyon_test WIN32
    whyon_test.cpp whyon_test.h
)
target_include_directories(whyon_test PRIVATE
    ../../../plugins/interfaces
    ../../src
    ../common
)

target_link_libraries(whyon_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
