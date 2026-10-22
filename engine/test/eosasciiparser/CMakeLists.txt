add_executable(eosasciiparser_test WIN32
    eosasciiparser_test.cpp eosasciiparser_test.h
)
target_include_directories(eosasciiparser_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)
target_compile_definitions(eosasciiparser_test PRIVATE
    EOS_SAMPLES_DIR="${CMAKE_SOURCE_DIR}/test-workspaces/eos-samples/"
)

target_link_libraries(eosasciiparser_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)
