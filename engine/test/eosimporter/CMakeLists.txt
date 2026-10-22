add_executable(eosimporter_test WIN32
    eosimporter_test.cpp eosimporter_test.h
)
target_include_directories(eosimporter_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)
target_compile_definitions(eosimporter_test PRIVATE
    EOS_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/test-workspaces/eos-samples/"
)

target_link_libraries(eosimporter_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
