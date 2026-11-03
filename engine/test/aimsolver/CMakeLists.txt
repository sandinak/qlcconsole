add_executable(aimsolver_test WIN32
    aimsolver_test.cpp aimsolver_test.h
)
target_include_directories(aimsolver_test PRIVATE
    ../../../plugins/interfaces
    ../../src
)

target_link_libraries(aimsolver_test PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Test
    qlcplusengine
)

# The two-arm beam a round-trip row aims (its indexed pan band, two heads).
target_compile_definitions(aimsolver_test PRIVATE
    AIMSOLVER_TEST_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
)
